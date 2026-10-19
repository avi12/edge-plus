// Unit tests for the assistant chat table (chat_target.h). No Edge/Win32 deps.
// Run via test\run.cmd.
#include <cstdio>
#include <cstring>
#include "../src/chat_target.h"

using namespace fixedge;
using namespace fixedge::chat_target;

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

bool IsFilled(const char* text) {
  return text && text[0] != '\0';
}

bool IsFullyDescribed(const Target& target) {
  return IsFilled(target.name) && IsFilled(target.answerSelectors) &&
         IsFilled(target.stopSelectors) && IsFilled(target.dialogSelectors) &&
         IsFilled(target.noticeText);
}

void RunChecks() {
  Check(std::strcmp(kCopilotTarget.name, "Copilot") == 0,
        "Copilot is called Copilot");
  Check(IsFullyDescribed(kCopilotTarget), "the row fills every column");
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
