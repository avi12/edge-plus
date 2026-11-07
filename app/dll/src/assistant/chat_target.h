// chat_target.h — which assistant "Ask" talks to, and the selectors
// chat_scripts.h reads its chat by. The web app changes without notice; this is
// the one place to fix when it does. Pure.
#pragma once

namespace edgeplus::chat_target {

// Each selector is a CSS selector list.
struct Target {
  const char* name = "";
  const char* answerSelectors = "";
  const char* stopSelectors = "";    // the button that stops an answer
  const char* dialogSelectors = "";  // a security check, a consent page
  // The box a question is typed into: once it is there, the page has loaded
  // far enough to take a question sent to it while hidden.
  const char* composerSelectors = "";
  const char* sendSelectors = "";  // the button that sends what was typed
  // The words of the first-use notice that covers the composer until the person
  // accepts it (we never accept it for them).
  const char* noticeText = "";
};

// What was verified is in docs/re/find-bar.md.
inline constexpr Target kCopilotTarget = {
    "Copilot",
    "[data-content=\"ai-message\"], [data-testid=\"ai-message\"], "
    "[data-testid=\"copilot-message\"]",
    "button[aria-label*=\"Stop\" i], [data-testid=\"stop-button\"]",
    "[role=\"dialog\"][aria-modal=\"true\"]",
    "#m365-chat-editor-target-element, textarea#userInput, "
    "[contenteditable=\"true\"][aria-label*=\"Message Copilot\" i], textarea",
    "button.fai-SendButton, button[type=\"submit\"][aria-label=\"Send\" i], "
    "button[data-testid=\"submit-button\"], "
    "button[aria-label=\"Submit message\" i]",
    "Navigate the Web with Copilot",
};

}  // namespace edgeplus::chat_target
