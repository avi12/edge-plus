// Unit tests for theme_hold.h.
#include <cstdio>
#include "../../src/theme/theme_hold.h"

using namespace edgeplus::theme_hold;

namespace {

struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

constexpr std::string_view kHeld = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr std::string_view kOther = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

void RunChecks() {
  Check(ShouldKeepOff(kHeld, kHeld, false),
        "Edge making the held theme active is stopped");
  Check(!ShouldKeepOff(kHeld, kOther, false), "any other theme goes through");
  Check(!ShouldKeepOff(kHeld, kHeld, true),
        "Edge+ switching to the held theme itself is let through");
  Check(!ShouldKeepOff({}, kHeld, false),
        "with nothing held, nothing is stopped");
  Check(!ShouldKeepOff({}, {}, false), "an empty id is never held");
}

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (const CheckFailed&) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
