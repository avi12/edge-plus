// Unit tests for the pure pane-transition math (motion.h). No Edge/Win32 deps.
// Run via test\run.cmd.
#include <cstdio>
#include <vector>
#include "../src/motion.h"

using namespace fixedge;
using namespace fixedge::motion;

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

// How many evenly spaced points the easing curve is sampled at, and how far a
// sample may dip below the previous one before it counts as non-monotonic.
constexpr int kEaseSamples = 100;
constexpr double kEaseSlack = 1e-9;
// How many steps a transition is checked at.
constexpr int kLerpSteps = 10;

void RunChecks() {
  // Easing: pinned ends, monotonic, front-loaded (fast start, soft landing).
  Check(Ease(0.0) == 0.0 && Ease(1.0) == 1.0, "ease pins 0 and 1");
  Check(Ease(-1.0) == 0.0 && Ease(2.0) == 1.0, "ease clamps out-of-range");
  {
    double previous = 0.0;
    bool isMonotonic = true;
    for (int i = 1; i <= kEaseSamples; i++) {
      const double eased = Ease(i / static_cast<double>(kEaseSamples));
      isMonotonic = isMonotonic && eased >= previous - kEaseSlack;
      previous = eased;
    }
    Check(isMonotonic, "ease is monotonic");
  }
  Check(Ease(0.25) > 0.6, "ease is front-loaded (snappy)");

  // Lerp: ends exact, midpoint halfway, adjacent panes stay seamless.
  {
    const Rect wide{0, 0, 800, 600};
    const Rect narrow{0, 0, 400, 600};
    Check(Same(Lerp(wide, narrow, 0.0), wide) &&
              Same(Lerp(wide, narrow, 1.0), narrow),
          "lerp ends exact");
    Check(Same(Lerp(wide, narrow, 0.5), Rect{0, 0, 600, 600}), "lerp midpoint");
    // A new pane growing in from the right edge, beside the shrinking one.
    const Rect enteringFrom{800, 0, 0, 600};
    const Rect enteringTo{400, 0, 400, 600};
    for (int i = 0; i <= kLerpSteps; i++) {
      const double progress = i / static_cast<double>(kLerpSteps);
      const Rect shrinking = Lerp(wide, narrow, progress);
      const Rect entering = Lerp(enteringFrom, enteringTo, progress);
      Check(shrinking.x + shrinking.width == entering.x,
            "neighbours stay seamless mid-flight");
      Check(entering.x + entering.width == 800,
            "new pane stays pinned to the right edge");
    }
  }

  // EntryRect: grows out of the split parent's edge on the carved side.
  {
    const std::vector<Rect> before{{0, 0, 800, 600}};
    Check(Same(EntryRect({400, 0, 400, 600}, before), Rect{800, 0, 0, 600}),
          "add-right enters from right edge");
    Check(Same(EntryRect({0, 0, 400, 600}, before), Rect{0, 0, 0, 600}),
          "add-left enters from left edge");
    Check(Same(EntryRect({0, 300, 800, 300}, before), Rect{0, 600, 800, 0}),
          "add-down enters from bottom edge");
    Check(Same(EntryRect({0, 0, 800, 300}, before), Rect{0, 0, 800, 0}),
          "add-up enters from top edge");
  }
  {
    // Parent is the right column of a 2-col split; hidden (empty) rects are skipped.
    const std::vector<Rect> before{
        {0, 0, 0, 0}, {0, 0, 400, 600}, {400, 0, 400, 600}};
    Check(Same(EntryRect({400, 300, 400, 300}, before), Rect{400, 600, 400, 0}),
          "add-down inside right column enters from its bottom edge");
  }
  Check(Same(EntryRect({100, 100, 200, 200}, {}), Rect{200, 200, 0, 0}),
        "no parent -> grows from centre");
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
