// Unit tests for tab_edge.h.
#include <cstdio>
#include "../../../src/chrome/tabs/tab_edge.h"

using namespace edgeplus;
using namespace edgeplus::tab_edge;

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

  Check(TrailingBottomCorner(Side::kLeft) == Corner::kLowerRight &&
            TrailingBottomCorner(Side::kRight) == Corner::kLowerLeft,
        "a tab trails on the side opposite its leading one");
  Check(
      onLeft.corners[static_cast<size_t>(TrailingBottomCorner(Side::kLeft))] ==
              round &&
          onRight.corners[static_cast<size_t>(
              TrailingBottomCorner(Side::kRight))] == round,
      "the radius sits on the trailing bottom corner either way");

  const float left = 10;
  const float top = 4;
  const float right = 250;
  const float bottom = 36;
  const float leg = 12;
  const CutOutline ltr =
      CutOutlineOf(left, top, right, bottom, leg, Side::kLeft);
  Check(ltr.points[0] == PointF{left, top} &&
            ltr.points[1] == PointF{right, top} &&
            ltr.points[2] == PointF{right, bottom - leg} &&
            ltr.points[3] == PointF{right - leg, bottom} &&
            ltr.points[4] == PointF{left, bottom} &&
            ltr.points[5] == ltr.points[0],
        "a left-leading outline is cut at its bottom right, straight on the "
        "left, and closes on its start");
  const CutOutline rtl =
      CutOutlineOf(left, top, right, bottom, leg, Side::kRight);
  Check(rtl.points[0] == PointF{right, top} &&
            rtl.points[1] == PointF{left, top} &&
            rtl.points[2] == PointF{left, bottom - leg} &&
            rtl.points[3] == PointF{left + leg, bottom} &&
            rtl.points[4] == PointF{right, bottom} &&
            rtl.points[5] == rtl.points[0],
        "a right-leading outline is cut at its bottom left, straight on the "
        "right");
  bool isMirror = true;
  for (size_t i = 0; i < kCutOutlinePointCount; i++) {
    const bool isMirrorPoint =
        rtl.points[i].x == left + right - ltr.points[i].x &&
        rtl.points[i].y == ltr.points[i].y;
    isMirror = isMirror && isMirrorPoint;
  }
  Check(isMirror,
        "the right-to-left outline is the left-to-right one flipped across the "
        "tab's middle");

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
