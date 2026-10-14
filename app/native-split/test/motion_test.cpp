// Unit tests for the pure pane-transition math (motion.h). No Edge/Win32 deps.
// Run via test\run.cmd.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/motion.h"

using namespace fixedge;
using namespace fixedge::motion;

namespace {

int g_checks = 0;
void Check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    std::exit(1);
  }
}

}  // namespace

int main() {
  // Easing: pinned ends, monotonic, front-loaded (fast start, soft landing).
  Check(Ease(0.0) == 0.0 && Ease(1.0) == 1.0, "ease pins 0 and 1");
  Check(Ease(-1.0) == 0.0 && Ease(2.0) == 1.0, "ease clamps out-of-range");
  {
    double prev = 0.0;
    bool mono = true;
    for (int i = 1; i <= 100; ++i) {
      const double v = Ease(i / 100.0);
      mono = mono && v >= prev - 1e-9;
      prev = v;
    }
    Check(mono, "ease is monotonic");
  }
  Check(Ease(0.25) > 0.6, "ease is front-loaded (snappy)");

  // Lerp: ends exact, midpoint halfway, adjacent panes stay seamless.
  {
    const Rect a{0, 0, 800, 600}, b{0, 0, 400, 600};
    Check(Same(Lerp(a, b, 0.0), a) && Same(Lerp(a, b, 1.0), b),
          "lerp ends exact");
    Check(Same(Lerp(a, b, 0.5), Rect{0, 0, 600, 600}), "lerp midpoint");
    const Rect na{800, 0, 0, 600},
        nb{400, 0, 400, 600};  // new pane from the right edge
    for (int i = 0; i <= 10; ++i) {
      const Rect l = Lerp(a, b, i / 10.0), r = Lerp(na, nb, i / 10.0);
      Check(l.x + l.w == r.x, "neighbours stay seamless mid-flight");
      Check(r.x + r.w == 800, "new pane stays pinned to the right edge");
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

  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
