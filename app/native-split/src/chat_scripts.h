// chat_scripts.h — the scripts that drive an assistant's web chat for "Ask".
// One concern: the JavaScript that puts a prompt into the chat's message box and
// presses send, and the JavaScript that reads the answer once it has stopped
// growing. Edge has no public API to ask Copilot a question, so this drives the
// signed-in web app the way a person would. What differs (selectors, name) is a
// chat_target::Target, the one table to fix when the web app changes. They run
// in our isolated world.
//
// The answer's markup is a guess backed by a fallback that needs no markup: the
// text that appears after the prompt. What was verified live is in
// docs/re/find-bar.md.
#pragma once
#include <string>
#include "chat_target.h"
#include "web_text.h"

namespace fixedge::chat_scripts {

using chat_target::Target;

// What the send script answers at once, and the reason it gives for failing.
inline constexpr char kScheduledAnswer[] = "scheduled";
inline constexpr char kNoBoxAnswer[] = "no-box";
// What the reply script answers while the answer is still arriving, and while
// a dialog (a security check, a consent page) waits for the person.
inline constexpr char kWaitAnswer[] = "wait";
inline constexpr char kUserAnswer[] = "user";
// What it answers when the page was replaced after the prompt went in (Copilot's
// landing page redirects to its chat page, and the new page has no memory of the
// prompt): the prompt has to be sent again.
inline constexpr char kReloadedAnswer[] = "reloaded";

// How long the answer must stay unchanged (and no stop button show) before it
// counts as finished.
inline constexpr int kSettleMs = 2500;
// How long the send script keeps looking for an enabled send button.
inline constexpr int kSendButtonWaitMs = 4000;
// How much of the end of the prompt is looked for on the page, to tell where
// the answer begins when the answer's own markup is not known.
inline constexpr int kEchoTailChars = 60;

// Both scripts share this: press the send button once it is enabled.
inline std::string PressSendFunction(const Target& target) {
  std::string script = R"JS(
  function pressSend(state, giveUpMs) {
    var deadline = Date.now() + giveUpMs;
    var timer = setInterval(function () {
      var button = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.sendSelectors);
  script += R"JS();
      var isReady = button && !button.disabled && button.getAttribute("aria-disabled") !== "true";
      if (isReady) {
        clearInterval(timer);
        button.click();
      } else if (!button && Date.now() > deadline) {
        // No send button at all (its markup changed): press Enter in the box.
        clearInterval(timer);
        var focused = document.activeElement;
        ["keydown", "keypress", "keyup"].forEach(function (type) {
          if (focused) {
            focused.dispatchEvent(new KeyboardEvent(type, { key: "Enter", code: "Enter", keyCode: 13, which: 13, bubbles: true }));
          }
        });
      } else if (Date.now() > deadline) {
        clearInterval(timer);
        state.failure = )JS";
  script += web_text::JsStringLiteral(std::string(target.name) +
                                      "'s send button did not wake up.");
  script += R"JS(;
      }
    }, 150);
  }
)JS";
  return script;
}

// Put `prompt` into the message box and press send once the button wakes. It
// answers at once: kScheduledAnswer, or kNoBoxAnswer when the page has no message
// box yet (still loading, signed out, or a consent page).
inline std::string SendScript(const Target& target, const std::string& prompt) {
  std::string script = "(function (prompt) {" + PressSendFunction(target);
  script += R"JS(
  var box = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.boxSelectors);
  script += R"JS();
  if (!box) {
    return ")JS";
  script += kNoBoxAnswer;
  script += R"JS(";
  }
  var state = window.__fxChat = {
    baseline: document.querySelectorAll()JS";
  script += web_text::JsStringLiteral(target.answerSelectors);
  script += R"JS().length,
    prompt: prompt, lastText: "", changedAt: Date.now(), failure: "", wasBlocked: false
  };
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
  pressSend(state, )JS";
  script += std::to_string(kSendButtonWaitMs);
  script += R"JS();
  return ")JS";
  script += kScheduledAnswer;
  script += R"JS(";
}))JS";
  script += "(" + web_text::JsStringLiteral(prompt) + ")";
  return script;
}

// Asked about once a second after sending: kWaitAnswer while the answer is
// missing or still being written, kUserAnswer while a dialog needs the person
// (and, once it is gone with no answer yet, the send is pressed again), then
// `ok US text` (ask_text::ParseReply's format), or `err US why`.
inline std::string ReplyScript(const Target& target) {
  std::string script = "(function () {" + PressSendFunction(target);
  script += R"JS(
  var state = window.__fxChat;
  if (!state) {
    return ")JS";
  script += kReloadedAnswer;
  script += R"JS(";
  }
  if (state.failure) {
    return "err\x1f" + state.failure;
  }
  var dialog = document.querySelector()JS";
  script += web_text::JsStringLiteral(target.dialogSelectors);
  script += R"JS();
  if (dialog && dialog.offsetWidth) {
    state.wasBlocked = true;
    return ")JS";
  script += kUserAnswer;
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
    if (state.wasBlocked) {
      state.wasBlocked = false;
      pressSend(state, )JS";
  script += std::to_string(kSendButtonWaitMs);
  script += R"JS();
    }
    return ")JS";
  script += kWaitAnswer;
  script += R"JS(";
  }
  if (text !== state.lastText) {
    state.lastText = text;
    state.changedAt = Date.now();
    return ")JS";
  script += kWaitAnswer;
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
  script += kWaitAnswer;
  script += R"JS(";
  }
  return "ok\x1f" + text;
})())JS";
  return script;
}

}  // namespace fixedge::chat_scripts
