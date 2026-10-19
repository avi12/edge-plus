// chat_target.h — which assistant "Ask" talks to. One concern, pure (no Edge, no
// Win32; unit-tested in test/chat_target_test.cpp): the assistant's display name
// and the selectors its chat is read by. The question goes to Copilot's side
// panel through Edge's own code (assistant_chat.h); the driver (chat_scripts.h)
// reads the panel's page with these. A web app is not ours and changes without
// notice, so this is the one place to fix when it does.
#pragma once

namespace fixedge::chat_target {

// What the assistant's chat page looks like to the reader: an answer, the button
// that stops one being written, and a dialog that needs the person (a security
// check, a consent page). Each is a CSS selector list.
struct Target {
  const char* name;  // the assistant's name, as the UI says it
  const char* answerSelectors;
  const char* stopSelectors;
  const char* dialogSelectors;
  // The words of the first-use notice that covers the composer until the person
  // accepts it (we never accept it for them).
  const char* noticeText;
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
