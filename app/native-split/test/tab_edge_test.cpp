// Unit tests for tab_edge.h.
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

constexpr float kRadius = 7;

void RunChecks() {
  Check(LeadingSide(false) == Side::kLeft && LeadingSide(true) == Side::kRight,
        "left to right leads on the left, right to left on the right");

  const CornerRadius round{kRadius, kRadius};
  const CornerRadius square{};
  const CornerRadii onLeft = RadiiFor(Side::kLeft, kRadius, true);
  Check(onLeft.corners[static_cast<size_t>(Corner::kUpperLeft)] == square &&
            onLeft.corners[static_cast<size_t>(Corner::kLowerLeft)] == square,
        "a left-leading tab is straight on its left");
  Check(onLeft.corners[static_cast<size_t>(Corner::kLowerRight)] == round &&
            onLeft.corners[static_cast<size_t>(Corner::kUpperRight)] == square,
        "a left-leading tab keeps the radius on its trailing bottom corner "
        "only");
  const CornerRadii onRight = RadiiFor(Side::kRight, kRadius, true);
  Check(
      onRight.corners[static_cast<size_t>(Corner::kUpperRight)] == square &&
          onRight.corners[static_cast<size_t>(Corner::kLowerRight)] == square &&
          onRight.corners[static_cast<size_t>(Corner::kUpperLeft)] == square &&
          onRight.corners[static_cast<size_t>(Corner::kLowerLeft)] == round,
      "a right-leading tab mirrors it");
  const CornerRadii uncut = RadiiFor(Side::kLeft, kRadius, false);
  Check(uncut.corners[static_cast<size_t>(Corner::kUpperRight)] == square &&
            uncut.corners[static_cast<size_t>(Corner::kLowerRight)] == square &&
            uncut.corners[static_cast<size_t>(Corner::kLowerLeft)] == square,
        "an uncut tab is straight on both sides");

  Check(IsTrailingCornerCut(false, false),
        "a tab of its own is cut, focused or not");
  Check(IsTrailingCornerCut(true, true), "the focused pane's tab is cut");
  Check(!IsTrailingCornerCut(true, false),
        "the split's other members are not, however many");
  Check(
      LeadingStrip(Rect{4, 0, 240, 32}, Side::kLeft, 7) == Rect{4, 0, 7, 32} &&
          LeadingStrip(Rect{4, 0, 240, 32}, Side::kRight, 7) ==
              Rect{237, 0, 7, 32},
      "the corner strip hugs the leading side");
  Check(LeadingStrip(Rect{0, 0, 5, 32}, Side::kLeft, 7) == Rect{0, 0, 5, 32},
        "a narrow rect is covered whole, not overshot");
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
