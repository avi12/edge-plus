// chat_target.h — which assistant "Ask" talks to, and the selectors
// chat_scripts.h reads its chat by. The web app changes without notice; this is
// the one place to fix when it does. Pure.
#pragma once

namespace fixedge::chat_target {

// Each selector is a CSS selector list.
struct Target {
  const char* name = "";
  const char* answerSelectors = "";
  const char* stopSelectors = "";    // the button that stops an answer
  const char* dialogSelectors = "";  // a security check, a consent page
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
    "Navigate the Web with Copilot",
};

}  // namespace fixedge::chat_target
