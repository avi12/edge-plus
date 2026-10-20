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

// The one-word answers the page scripts give besides `ok US text` / `err US why`.
enum class ScriptAnswer {
  kWait,  // the answer is still arriving
  kUser,  // a dialog (a security check, a consent page) waits for the person
  kReloaded,  // the page was replaced after the prompt went in (the side panel
              // reloads): the question has to be sent again
  kArmed,     // what the arm script answers
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
  }
  return "";
}

// How long the answer must stay unchanged (and no stop button show) before it
// counts as finished.
inline constexpr int kSettleMs = 2500;
// How much of the end of the prompt is looked for on the page, to tell where
// the answer begins when the answer's own markup is not known.
inline constexpr int kEchoTailChars = 60;

// Run in the side panel's page once the question was sent: remember what the
// page held before the answer (how many answers there were) and the question,
// so the reply script can tell the new answer from the old page.
inline std::string ArmScript(const Target& target, const std::string& prompt) {
  std::string script = R"JS((function (prompt) {
  window.__fxChat = {
    baseline: document.querySelectorAll()JS";
  script += web_text::JsStringLiteral(target.answerSelectors);
  script += R"JS().length,
    prompt: prompt, lastText: "", changedAt: Date.now(), failure: ""
  };
  return ")JS";
  script += AnswerText(ScriptAnswer::kArmed);
  script += R"JS(";
}))JS";
  script += "(" + web_text::JsStringLiteral(prompt) + ")";
  return script;
}

// Asked about once a second after sending: kWait while the answer is
// missing or still being written, kUser while a dialog needs the person,
// kReloaded when the page lost the question, then
// `ok US text` (ask_text::ParseReply's format), or `err US why`.
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

}  // namespace fixedge::chat_scripts
