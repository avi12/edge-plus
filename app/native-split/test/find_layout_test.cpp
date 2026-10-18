// Unit tests for the find bar's layout (find_layout.h). No Edge/Win32 deps. Run
// via test\run.cmd.
#include <cstdio>
#include "../src/find_layout.h"

using namespace fixedge;
using namespace fixedge::find_layout;

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
  const Rect wide = BarRect(Rect{0, 100, 1400, 800});
  Check(wide.width == kBarWidth && wide.height == kBarHeight,
        "a roomy page gets the full bar");
  Check(wide.x == 1400 - kRightGap - kBarWidth && wide.y == 100 + kTopGap,
        "top right of the page, clear of its scroll bar");

  const Rect offset = BarRect(Rect{300, 0, 1000, 800});
  Check(offset.x + offset.width == 300 + 1000 - kRightGap,
        "relative to the page, not the window");

  const Rect narrow = BarRect(Rect{0, 0, 300, 800});
  Check(narrow.width == 300 - kRightGap && narrow.x == 0,
        "a narrow page gives the bar the width that is left");

  const Rect tiny = BarRect(Rect{0, 0, 10, 10});
  Check(tiny.width == 0 && tiny.height == 4, "never negative");
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
