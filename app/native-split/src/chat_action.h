// chat_action.h — what the chat button's dropdown items do. One concern, pure
// (no Edge, no Win32; unit-tested in test/chat_action_test.cpp): the three
// things the menu offers (open the assistant, summarize the page, talk with the
// page shared), named once, and the ids Edge gives their menu items.
#pragma once

namespace fixedge::chat_action {

enum class ChatAction {
  kNone,       // nothing pending, or an item that is not ours (a nudge)
  kChat,       // "Chat": open the assistant
  kSummarize,  // "Summarize page": share the tab and ask for a summary
  kTalk,       // "Talk to ... + Share tab": share the tab and start voice
};

// The command ids of EdgeCopilotMenuModel's items (BuildMenu adds them).
enum class MenuCommand : int {
  kChat = 0,
  kTalkAndShare = 1,
  kNudge = 2,
  kSummarize = 3,
};

inline ChatAction ActionFor(int commandId) {
  if (commandId == static_cast<int>(MenuCommand::kChat)) {
    return ChatAction::kChat;
  }
  if (commandId == static_cast<int>(MenuCommand::kTalkAndShare)) {
    return ChatAction::kTalk;
  }
  if (commandId == static_cast<int>(MenuCommand::kSummarize)) {
    return ChatAction::kSummarize;
  }
  return ChatAction::kNone;
}

// What Gemini is asked after the page is shared for a summary, and the label
// of the voice item with Gemini's name in it.
inline constexpr char kSummarizeInstruction[] = "Summarize this page.";
inline constexpr char kSummarizePrompt[] = "Summarize this page";
inline constexpr char kTalkLabel[] = "Talk to Gemini + Share tab";
inline constexpr char kHideLabel[] = "Hide Gemini";

}  // namespace fixedge::chat_action
