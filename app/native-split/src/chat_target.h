// chat_target.h — which web chat "Ask" drives. One concern, pure (no Edge, no
// Win32; unit-tested in test/chat_target_test.cpp): the assistant's display
// name, the page that is opened for it and the selectors its chat page is
// driven by. The driver (chat_scripts.h, assistant_chat.h) reads this table; a
// web app is not ours and changes without notice, so this is the one place to
// fix when it does.
#pragma once

namespace fixedge::chat_target {

// What the assistant's chat page looks like to the driver: where it lives, the
// message box, the send button, an answer, the button that stops one being
// written, and a dialog that needs the person (a security check, a consent
// page). Each is a CSS selector list.
struct Target {
  const char* name;  // the assistant's name, as the UI says it
  const char* url;
  const char* boxSelectors;
  const char* sendSelectors;
  const char* answerSelectors;
  const char* stopSelectors;
  const char* dialogSelectors;
};

// What was verified is in docs/re/find-bar.md.
inline constexpr Target kCopilotTarget = {
    "Copilot",
    "https://copilot.microsoft.com/",
    "#m365-chat-editor-target-element, textarea#userInput, "
    "[contenteditable=\"true\"][aria-label*=\"Message Copilot\" i]",
    "button.fai-SendButton, button[type=\"submit\"][aria-label=\"Send\" i], "
    "button[data-testid=\"submit-button\"], "
    "button[aria-label=\"Submit message\" i]",
    "[data-content=\"ai-message\"], [data-testid=\"ai-message\"], "
    "[data-testid=\"copilot-message\"]",
    "button[aria-label*=\"Stop\" i], [data-testid=\"stop-button\"]",
    "[role=\"dialog\"][aria-modal=\"true\"]",
};

}  // namespace fixedge::chat_target
