// Unit tests for the chat menu's actions (chat_action.h). No Edge/Win32 deps.
// Run via test\run.cmd.
#include <cstdio>
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
