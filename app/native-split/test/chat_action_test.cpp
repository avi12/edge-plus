// Unit tests for the chat menu's actions (chat_action.h). No Edge/Win32 deps.
// Run via test\run.cmd.
#include <cstdio>
#include <string>
#include "../src/chat_action.h"

using namespace fixedge::chat_action;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

void RunChecks() {
  Check(ActionFor(0) == ChatAction::kChat, "command 0 is Chat");
  Check(ActionFor(1) == ChatAction::kTalk, "command 1 is the voice item");
  Check(ActionFor(3) == ChatAction::kSummarize, "command 3 is Summarize page");
  Check(ActionFor(2) == ChatAction::kNone, "the nudge item is not ours");
  Check(ActionFor(99) == ChatAction::kNone, "an unknown command is not ours");

  Check(IsPageQuestion(ChatAction::kSummarize),
        "a summary asks about the page");
  Check(IsPageQuestion(ChatAction::kPodcast), "a podcast asks about the page");
  Check(!IsPageQuestion(ChatAction::kTalk), "a talk is not a page question");
  Check(!IsPageQuestion(ChatAction::kChat), "chat is not a page question");
  Check(
      std::string(InstructionFor(ChatAction::kPodcast)) == kPodcastInstruction,
      "the podcast instruction is the podcast's");
  Check(std::string(InstructionFor(ChatAction::kSummarize)) ==
            kSummarizeInstruction,
        "the summary instruction is the summary's");
  Check(std::string(PromptFor(ChatAction::kPodcast)) == kPodcastPrompt,
        "the podcast prompt is the podcast's");
  Check(std::string(InstructionFor(ChatAction::kTalk)).empty(),
        "a talk appends nothing to the page");
  Check(std::string(PromptFor(ChatAction::kNone)).empty(),
        "no action sends no prompt");
}

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
