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
  Check(onLeft.corners[static_cast<size_t>(Corner::kUpperRight)] == round &&
            onLeft.corners[static_cast<size_t>(Corner::kLowerLeft)] == round,
        "a left-leading tab keeps the radius on its top right and bottom left");
  Check(onLeft.corners[static_cast<size_t>(Corner::kUpperLeft)] == square &&
            onLeft.corners[static_cast<size_t>(Corner::kLowerRight)] == square,
        "and is square on its top left and bottom right");
  const CornerRadii onRight = RadiiFor(Side::kRight, kRadius, true);
  Check(
      onRight.corners[static_cast<size_t>(Corner::kUpperLeft)] == round &&
          onRight.corners[static_cast<size_t>(Corner::kLowerRight)] == round &&
          onRight.corners[static_cast<size_t>(Corner::kUpperRight)] == square &&
          onRight.corners[static_cast<size_t>(Corner::kLowerLeft)] == square,
      "a right-leading tab mirrors it");
  const CornerRadii uncut = RadiiFor(Side::kLeft, kRadius, false);
  bool isAllSquare = true;
  for (const CornerRadius& corner : uncut.corners) {
    isAllSquare = isAllSquare && corner == square;
  }
  Check(isAllSquare, "an uncut tab is square at every corner");

  Check(TrailingTopCorner(Side::kLeft) == Corner::kUpperRight &&
            TrailingTopCorner(Side::kRight) == Corner::kUpperLeft,
        "the top cut is on the side a tab trails to");
  Check(LeadingBottomCorner(Side::kLeft) == Corner::kLowerLeft &&
            LeadingBottomCorner(Side::kRight) == Corner::kLowerRight,
        "the bottom cut is on its leading side");

  const float left = 10;
  const float top = 4;
  const float right = 250;
  const float bottom = 36;
  const float leg = 12;
  const CutOutline ltr =
      CutOutlineOf(left, top, right, bottom, leg, Side::kLeft);
  Check(ltr.points[0] == PointF{left, top} &&
            ltr.points[1] == PointF{right - leg, top} &&
            ltr.points[2] == PointF{right, top + leg} &&
            ltr.points[3] == PointF{right, bottom} &&
            ltr.points[4] == PointF{left + leg, bottom} &&
            ltr.points[5] == PointF{left, bottom - leg} &&
            ltr.points[6] == ltr.points[0],
        "a left-leading plate is cut at its top right and bottom left, and "
        "closes on its start");
  const CutOutline rtl =
      CutOutlineOf(left, top, right, bottom, leg, Side::kRight);
  Check(rtl.points[0] == PointF{right, top} &&
            rtl.points[1] == PointF{left + leg, top} &&
            rtl.points[2] == PointF{left, top + leg} &&
            rtl.points[3] == PointF{left, bottom} &&
            rtl.points[4] == PointF{right - leg, bottom} &&
            rtl.points[5] == PointF{right, bottom - leg} &&
            rtl.points[6] == rtl.points[0],
        "a right-leading plate is cut at its top left and bottom right");
  const CutOutline flat =
      CutOutlineOf(left, top, right, bottom, 0, Side::kLeft);
  Check(flat.points[1] == PointF{right, top} &&
            flat.points[2] == PointF{right, top} &&
            flat.points[4] == PointF{left, bottom} &&
            flat.points[5] == PointF{left, bottom},
        "a leg of 0 leaves a rectangle");
  for (size_t i = 1; i < kCutOutlinePointCount; i++) {
    const PointF from = ltr.points[i - 1];
    const PointF to = ltr.points[i];
    const float across = to.x > from.x ? to.x - from.x : from.x - to.x;
    const float down = to.y > from.y ? to.y - from.y : from.y - to.y;
    const bool isStraightOrDiagonal =
        across == 0 || down == 0 || across == down;
    Check(isStraightOrDiagonal,
          "every edge is level, upright or at 45 degrees");
  }
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

  Check(IsPlateCut(false, false), "a tab of its own is cut, focused or not");
  Check(IsPlateCut(true, true), "the focused pane's tab is cut");
  Check(!IsPlateCut(true, false),
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
