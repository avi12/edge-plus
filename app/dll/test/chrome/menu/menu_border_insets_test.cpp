// Unit tests for menu_border_insets.h.
#include <cstdio>
#include "../../../src/chrome/menu/menu_border_insets.h"

using namespace edgeplus::menu_border_insets;

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
  const Insets menuAsEdgeSizesIt{0, 12, 28, 12};
  Check(WithRoomForTopLine(menuAsEdgeSizesIt) == Insets{1, 12, 28, 12},
        "a top with no room is given the line's pixel, the other sides stay");

  const Insets roomyTop{9, 12, 28, 12};
  Check(WithRoomForTopLine(roomyTop) == roomyTop,
        "a top that already has room keeps Edge's answer");

  const Insets exactlyOnePixel{1, 0, 0, 0};
  Check(WithRoomForTopLine(exactlyOnePixel) == exactlyOnePixel,
        "a top of exactly the line's thickness is left alone");

  const Insets negativeTop{-3, 4, 5, 6};
  Check(WithRoomForTopLine(negativeTop) == Insets{1, 4, 5, 6},
        "a negative top is raised to the line's pixel");
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
