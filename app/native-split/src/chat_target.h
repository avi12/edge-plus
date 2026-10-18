// chat_target.h — which web chat "Ask" drives. One concern, pure (no Edge, no
// Win32; unit-tested in test/chat_target_test.cpp): for each browser assistant
// (config.h) its display name, the page that is opened for it and the selectors
// its chat page is driven by. The driver (chat_scripts.h, assistant_chat.h) is
// the same for every assistant and reads this table; a web app is not ours and
// changes without notice, so these are the one place to fix when one does.
#pragma once
#include "config.h"

namespace fixedge::chat_target {

using config::Assistant;

// What one assistant's chat page looks like to the driver: where it lives, the
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

// What was verified for each is in docs/re/find-bar.md.
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

inline constexpr Target kGeminiTarget = {
    "Gemini",
    "https://gemini.google.com/app",
    "rich-textarea .ql-editor[contenteditable=\"true\"], "
    "[contenteditable=\"true\"][aria-label*=\"prompt\" i]",
    "button[aria-label=\"Send message\" i]",
    "message-content, model-response",
    "button[aria-label*=\"Stop\" i], button.stop",
    "[role=\"dialog\"][aria-modal=\"true\"], mat-dialog-container",
};

inline constexpr const Target& For(Assistant assistant) {
  return assistant == Assistant::kGemini ? kGeminiTarget : kCopilotTarget;
}

// The assistant's name, for every place the UI mentions it.
inline constexpr const char* Name(Assistant assistant) {
  return For(assistant).name;
}

}  // namespace fixedge::chat_target
