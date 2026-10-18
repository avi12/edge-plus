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
  kPodcast,    // the page menu's "Create podcast": share the tab, ask for audio
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

// What Gemini is asked after the page is shared (the text appended to the
// shared page), and what it is sent when it shares the tab itself (glic), for
// each action that works on the page; and the label of the voice item with
// Gemini's name in it.
inline constexpr char kSummarizeInstruction[] = "Summarize this page.";
inline constexpr char kSummarizePrompt[] = "Summarize this page";
inline constexpr char kPodcastInstruction[] =
    "Create a podcast (an Audio Overview) of this page.";
inline constexpr char kPodcastPrompt[] =
    "Create a podcast (an Audio Overview) of this page";

// Whether the action shares the active tab and then asks Gemini something
// about it (a talk shares it and starts voice instead).
inline bool IsPageQuestion(ChatAction action) {
  return action == ChatAction::kSummarize || action == ChatAction::kPodcast;
}

// The text appended to the shared page for a page question; empty for others.
inline const char* InstructionFor(ChatAction action) {
  if (action == ChatAction::kSummarize) {
    return kSummarizeInstruction;
  }
  if (action == ChatAction::kPodcast) {
    return kPodcastInstruction;
  }
  return "";
}

// The prompt glic is invoked with for a page question; empty for others.
inline const char* PromptFor(ChatAction action) {
  if (action == ChatAction::kSummarize) {
    return kSummarizePrompt;
  }
  if (action == ChatAction::kPodcast) {
    return kPodcastPrompt;
  }
  return "";
}
inline constexpr char kTalkLabel[] = "Talk to Gemini + Share tab";
inline constexpr char kHideLabel[] = "Hide Gemini";

}  // namespace fixedge::chat_action
