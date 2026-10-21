// Unit tests for the selected tab's spine and the tab's corner geometry
// (tab_edge.h). No
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
constexpr float kRadius = 7;

void RunChecks() {
  Check(LeadingSide(false) == Side::kLeft && LeadingSide(true) == Side::kRight,
        "left to right leads on the left, right to left on the right");

  const Rect left = StripeFor(kTab, Side::kLeft, kStripe);
  Check(left == Rect{4, 121, 7, 32},
        "the left stripe runs the row's full height from its left edge");
  const Rect right = StripeFor(kTab, Side::kRight, kStripe);
  Check(right == Rect{237, 121, 7, 32},
        "the right stripe ends on the row's right edge");
  Check(left.width == right.width && left.height == right.height,
        "both sides draw the same stripe, mirrored");

  const CornerRadius round{kRadius, kRadius};
  const CornerRadius square{};
  const CornerRadii onLeft = RadiiFor(Side::kLeft, kRadius, false);
  Check(onLeft.corners[static_cast<size_t>(Corner::kUpperLeft)] == square &&
            onLeft.corners[static_cast<size_t>(Corner::kLowerLeft)] == square,
        "a left-leading tab is straight on its left");
  Check(onLeft.corners[static_cast<size_t>(Corner::kUpperRight)] == round &&
            onLeft.corners[static_cast<size_t>(Corner::kLowerRight)] == round,
        "a left-leading tab keeps its right corners round");
  const CornerRadii onRight = RadiiFor(Side::kRight, kRadius, false);
  Check(
      onRight.corners[static_cast<size_t>(Corner::kUpperRight)] == square &&
          onRight.corners[static_cast<size_t>(Corner::kLowerRight)] == square &&
          onRight.corners[static_cast<size_t>(Corner::kUpperLeft)] == round &&
          onRight.corners[static_cast<size_t>(Corner::kLowerLeft)] == round,
      "a right-leading tab mirrors it");
  const CornerRadii joined = RadiiFor(Side::kLeft, kRadius, true);
  Check(
      joined.corners[static_cast<size_t>(Corner::kUpperRight)] == square &&
          joined.corners[static_cast<size_t>(Corner::kLowerRight)] == square &&
          joined.corners[static_cast<size_t>(Corner::kUpperLeft)] == square,
      "a split member with a neighbour is straight on both sides");

  // A split's row: two members side by side, 120 across each, in one row.
  constexpr Rect kFirst{4, 0, 120, 32};
  constexpr Rect kSecond{124, 0, 120, 32};
  constexpr Rect kElsewhere{4, 40, 240, 32};
  const Rect others[] = {kSecond, kElsewhere};
  Check(HasTrailingNeighbor(kFirst, Side::kLeft, others, 2),
        "the first member has the second against its right");
  const Rect before[] = {kFirst, kElsewhere};
  Check(!HasTrailingNeighbor(kSecond, Side::kLeft, before, 2),
        "the last member has nothing against its right");
  Check(HasTrailingNeighbor(kSecond, Side::kRight, before, 2),
        "in a right-to-left row the trailing side is the left one");
  Check(!HasTrailingNeighbor(kElsewhere, Side::kLeft, others, 1),
        "a tab on a row of its own has no neighbour");
  Check(
      LeadingStrip(Rect{4, 0, 240, 32}, Side::kLeft, 7) == Rect{4, 0, 7, 32} &&
          LeadingStrip(Rect{4, 0, 240, 32}, Side::kRight, 7) ==
              Rect{237, 0, 7, 32},
      "the corner strip hugs the leading side");
  Check(LeadingStrip(Rect{0, 0, 5, 32}, Side::kLeft, 7) == Rect{0, 0, 5, 32},
        "a narrow rect is covered whole, not overshot");
  Check(StripeColorFor(theme_tone::Tone::kDark) == 0xffff2bd6,
        "a dark theme keeps the neon magenta");
  Check(StripeColorFor(theme_tone::Tone::kLight) == 0xffc8008f,
        "a light theme gets the deep magenta");
  Check(StripeColorFor(theme_tone::Tone::kUnknown) ==
            StripeColorFor(theme_tone::Tone::kDark),
        "a theme of no known tone keeps the neon");
  Check(StripeColorFor(theme_tone::ToneOf({true, 0xffd5e4f0}, {})) ==
            kStripeOnLight,
        "the light Cyberpunk theme's concrete toolbar reads as light");
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
