// Unit tests for the selected tab's spine and corner geometry (tab_edge.h). No
// Edge/Win32 deps. Run via test\run.cmd.
#include <cstdio>
#include "../src/tab_edge.h"

using namespace fixedge;
using namespace fixedge::tab_edge;

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

// A tab row as seen live at 100%: 240 wide, 32 tall, 4 in from the screen edge.
constexpr Rect kTab{4, 121, 240, 32};
constexpr int kStripe = 7;
constexpr int kCorner = 8;

void RunChecks() {
  Check(LeadingSide(false) == Side::kLeft && LeadingSide(true) == Side::kRight,
        "left to right leads on the left, right to left on the right");

  const Pieces left = PiecesFor(kTab, Side::kLeft, kStripe, kCorner);
  Check(left.stripe == Rect{4, 121, 7, 32},
        "the left stripe runs the row's full height from its left edge");
  Check(left.topCorner == Rect{11, 121, 1, 8},
        "the top-left square starts where the stripe ends");
  Check(left.bottomCorner == Rect{11, 145, 1, 8},
        "the bottom-left square sits on the row's bottom edge");

  const Pieces right = PiecesFor(kTab, Side::kRight, kStripe, kCorner);
  Check(right.stripe == Rect{237, 121, 7, 32},
        "the right stripe ends on the row's right edge");
  Check(right.topCorner == Rect{236, 121, 1, 8},
        "the top-right square ends where the stripe starts");
  Check(right.bottomCorner == Rect{236, 145, 1, 8},
        "the bottom-right square sits on the row's bottom edge");

  const Pieces thin = PiecesFor(kTab, Side::kRight, 3, kCorner);
  Check(thin.topCorner == Rect{236, 121, 5, 8} &&
            thin.topCorner.x + thin.topCorner.width == thin.stripe.x,
        "a thinner stripe leaves a wider square, flush against it");
  Check(left.stripe.width == right.stripe.width &&
            left.topCorner.height == right.topCorner.height,
        "both sides draw the same pieces, mirrored");
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
