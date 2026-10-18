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
  return IsFilled(target.name) && IsFilled(target.url) &&
         IsFilled(target.boxSelectors) && IsFilled(target.sendSelectors) &&
         IsFilled(target.answerSelectors) && IsFilled(target.stopSelectors) &&
         IsFilled(target.dialogSelectors);
}

void RunChecks() {
  Check(std::strcmp(Name(Assistant::kCopilot), "Copilot") == 0,
        "Copilot is called Copilot");
  Check(std::strcmp(Name(Assistant::kGemini), "Gemini") == 0,
        "Gemini is called Gemini, not 'Ask Gemini'");
  Check(&For(Assistant::kCopilot) == &kCopilotTarget &&
            &For(Assistant::kGemini) == &kGeminiTarget,
        "each assistant has its own row");
  Check(IsFullyDescribed(For(Assistant::kCopilot)) &&
            IsFullyDescribed(For(Assistant::kGemini)),
        "every row fills every column");
  Check(std::strstr(For(Assistant::kCopilot).url, "copilot.microsoft.com") !=
                nullptr &&
            std::strstr(For(Assistant::kGemini).url, "gemini.google.com/app") !=
                nullptr,
        "each row opens its own web app");
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
