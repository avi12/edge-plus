// Unit tests for where the assistant's chat page sits (find_layout.h). No
// Edge/Win32 deps. Run via test\run.cmd.
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
  const Rect roomy = ChatRect(Rect{0, 100, 1400, 800});
  Check(roomy.width == kChatWidth && roomy.height == kChatHeight,
        "a roomy page gets the full card");
  Check(
      roomy.x == 1400 - kRightGap - kChatWidth && roomy.y == 100 + kTopGap,
      "top right of the page, under Edge's find bar, clear of the scroll bar");

  const Rect offset = ChatRect(Rect{300, 0, 1000, 800});
  Check(offset.x + offset.width == 300 + 1000 - kRightGap,
        "relative to the page, not the window");

  const Rect narrow = ChatRect(Rect{0, 0, 300, 800});
  Check(narrow.width == 300 - kRightGap && narrow.x == 0,
        "a narrow page gives the card the width that is left");

  const Rect shortPage = ChatRect(Rect{0, 0, 1400, 300});
  Check(shortPage.y + shortPage.height <= 300 - kBottomGap,
        "it never runs off a short page");
  const Rect none = ChatRect(Rect{0, 0, 1400, 50});
  Check(none.height == 0, "no room, no page");
  const Rect tiny = ChatRect(Rect{0, 0, 10, 10});
  Check(tiny.width == 0 && tiny.height == 0, "never negative");
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
