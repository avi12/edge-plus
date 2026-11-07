// chat_scripts.h — the scripts that read an assistant's web chat answer for
// "Ask", in our isolated world. Edge has no API that returns the answer. When
// the web app changes, fix its chat_target::Target.
//
// The answer's markup is a guess backed by a fallback that needs no markup: the
// text that appears after the prompt. What was verified live is in
// docs/re/find-bar.md.
#pragma once
#include <string>
#include "assistant/chat_target.h"
#include "find/web_text.h"

namespace edgeplus::chat_scripts {

using chat_target::Target;

// The one-word page-script answers besides `ok US text` / `err US why`.
enum class ScriptAnswer {
  kWait,
  kUser,  // a dialog (a security check, a consent page) waits for the person
  kReloaded,  // the side panel reloaded after the prompt went in: send it again
  kArmed,
  kStopped,  // the stop button was clicked
};

inline const char* AnswerText(ScriptAnswer answer) {
  switch (answer) {
    case ScriptAnswer::kWait:
      return "wait";
    case ScriptAnswer::kUser:
      return "user";
    case ScriptAnswer::kReloaded:
      return "reloaded";
    case ScriptAnswer::kArmed:
      return "armed";
    case ScriptAnswer::kStopped:
      return "stopped";
  }
  return "";
}

// How long the answer must stay unchanged (and no stop button show).
inline constexpr int kSettleMs = 2500;
// The prompt's tail looked for on the page, to tell where the answer begins
// when the answer's own markup is not known.
inline constexpr int kEchoTailChars = 60;

// How the question reaches the chat page the arm script runs in.
enum class Delivery {
  kAlreadyIn,  // Edge put it in (the panel path): only remember the baseline
  kByEdge,     // the hidden page is the chat itself: Edge's dispatch follows
  kTyped,      // the hidden chat is an iframe Edge cannot dispatch to: type it
};

// How long the typed question waits for the send button to wake.
inline constexpr int kSendButtonWaitMs = 4000;

// Remembers how many answers the page held before, so the reply script can
// tell the new answer from the old page. For a question that still has to go
// into the hidden page it first waits for the composer (kWait) and answers
// kUser while the first-use notice covers it (the question then goes through
// the panel, where the person can accept it). kTyped then types the question
// into the composer and presses send, as a person would.
inline std::string ArmScript(const Target& target, const std::string& prompt,
                             Delivery delivery) {
  const bool isHidden = delivery != Delivery::kAlreadyIn;
  const bool isTyped = delivery == Delivery::kTyped;
  std::string script = R"JS((function (prompt, isHidden, isTyped) {
  var box = null;
  if (isHidden) {
    var dialog = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.dialogSelectors);
  script += R"JS();
    var text = document.body ? document.body.innerText : "";
    if ((dialog && dialog.offsetWidth) || text.indexOf()JS";
  script += web_text::JsStringLiteral(target.noticeText);
  script += R"JS() >= 0) {
      return ")JS";
  script += AnswerText(ScriptAnswer::kUser);
  script += R"JS(";
    }
    box = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.composerSelectors);
  script += R"JS();
    if (document.readyState !== "complete" || !box) {
      return ")JS";
  script += AnswerText(ScriptAnswer::kWait);
  script += R"JS(";
    }
  }
  var state = window.__fxChat = {
    baseline: document.querySelectorAll()JS";
  script += web_text::JsStringLiteral(target.answerSelectors);
  script += R"JS().length,
    prompt: prompt, lastText: "", changedAt: Date.now(), failure: ""
  };
  if (isTyped) {
    box.focus();
    if (box.isContentEditable) {
      document.execCommand("selectAll", false);
      prompt.split("\n").forEach(function (line, index) {
        if (index > 0) {
          document.execCommand("insertLineBreak", false);
        }
        if (line) {
          document.execCommand("insertText", false, line);
        }
      });
    } else {
      var setter = Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype, "value").set;
      setter.call(box, prompt);
      box.dispatchEvent(new Event("input", { bubbles: true }));
    }
    var deadline = Date.now() + )JS";
  script += std::to_string(kSendButtonWaitMs);
  script += R"JS(;
    var timer = setInterval(function () {
      var button = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.sendSelectors);
  script += R"JS();
      var isReady = button && !button.disabled && button.getAttribute("aria-disabled") !== "true";
      if (isReady) {
        clearInterval(timer);
        button.click();
      } else if (Date.now() > deadline) {
        clearInterval(timer);
        state.failure = "the chat's send button did not wake up.";
      }
    }, 150);
  }
  return ")JS";
  script += AnswerText(ScriptAnswer::kArmed);
  script += R"JS(";
}))JS";
  script += "(" + web_text::JsStringLiteral(prompt) + ", " +
            (isHidden ? "true" : "false") + ", " +
            (isTyped ? "true" : "false") + ")";
  return script;
}

// Answers in ask_text::ParseReply's format once settled.
inline std::string ReplyScript(const Target& target) {
  std::string script = "(function () {";
  script += R"JS(
  var state = window.__fxChat;
  if (!state) {
    return ")JS";
  script += AnswerText(ScriptAnswer::kReloaded);
  script += R"JS(";
  }
  if (state.failure) {
    return )JS";
  script += web_text::JsErrorAnswerFrom("state.failure");
  script += R"JS(;
  }
  var dialog = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.dialogSelectors);
  script += R"JS();
  var isNoticeShown = document.body.innerText.indexOf()JS";
  script += web_text::JsStringLiteral(target.noticeText);
  script += R"JS() >= 0;
  if ((dialog && dialog.offsetWidth) || isNoticeShown) {
    return ")JS";
  script += AnswerText(ScriptAnswer::kUser);
  script += R"JS(";
  }
  var squeeze = function (text) { return String(text).replace(/\s+/g, " ").trim(); };
  var answers = document.querySelectorAll()JS";
  script += web_text::JsStringLiteral(target.answerSelectors);
  script += R"JS();
  var text = null;
  if (answers.length > state.baseline) {
    text = squeeze(answers[answers.length - 1].innerText || "");
  } else {
    var page = squeeze(document.body.innerText || "");
    var tail = squeeze(state.prompt).slice(-)JS";
  script += std::to_string(kEchoTailChars);
  script += R"JS();
    var echo = page.lastIndexOf(tail);
    if (echo >= 0) {
      text = page.slice(echo + tail.length).trim();
    }
  }
  if (text === null) {
    return ")JS";
  script += AnswerText(ScriptAnswer::kWait);
  script += R"JS(";
  }
  if (text !== state.lastText) {
    state.lastText = text;
    state.changedAt = Date.now();
    return ")JS";
  script += AnswerText(ScriptAnswer::kWait);
  script += R"JS(";
  }
  var isWriting = !!document.querySelector()JS";
  script += web_text::JsStringLiteral(target.stopSelectors);
  script += R"JS();
  var isSettled = Date.now() - state.changedAt >= )JS";
  script += std::to_string(kSettleMs);
  script += R"JS(;
  if (!text || isWriting || !isSettled) {
    return ")JS";
  script += AnswerText(ScriptAnswer::kWait);
  script += R"JS(";
  }
  return )JS";
  script += web_text::JsOkAnswer({"text"});
  script += R"JS(;
})())JS";
  return script;
}

// Clicks the button that stops the answer being written, if it shows.
inline std::string StopScript(const Target& target) {
  std::string script = R"JS((function () {
  var stop = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.stopSelectors);
  script += R"JS();
  if (stop && stop.offsetWidth) {
    stop.click();
    return ")JS";
  script += AnswerText(ScriptAnswer::kStopped);
  script += R"JS(";
  }
  return ")JS";
  script += AnswerText(ScriptAnswer::kWait);
  script += R"JS(";
})())JS";
  return script;
}

}  // namespace edgeplus::chat_scripts
