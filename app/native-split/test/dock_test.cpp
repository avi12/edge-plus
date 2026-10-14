// Unit tests for the drop-zone resolver (dock.h). No Edge/Win32 deps. Run via
// test\run.cmd.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../src/dock.h"

using namespace fixedge;
using namespace fixedge::dock;

namespace {

int g_checks = 0;
void Check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    std::exit(1);
  }
}

bool Is(const Rect& r, int x, int y, int w, int h) {
  return r.x == x && r.y == y && r.width == w && r.height == h;
}

}  // namespace

int main() {
  // A 1200x800 split holding one row of four 300-wide panes; we drag pane 3.
  const Rect layout{0, 0, 1200, 800};
  const Node row = FlatTree(4, Axis::kHorizontal);
  const Plan none;

  // Scenario (a): the bottom of pane 2 (above the outer band) docks below it;
  // the preview is pane 3's final rect: the bottom half of a third column.
  {
    const Plan p = Resolve(row, layout, Point{750, 700}, 3, none);
    Check(p.zone == Zone::kPaneEdge && p.target == 2 &&
              p.direction == Direction::kDown,
          "bottom of pane 2 -> below 2");
    Check(Is(p.preview, 800, 400, 400, 400),
          "preview = bottom half of the third column");
  }
  // Scenario (b): within the outer band at the bottom -> the full bottom half.
  {
    const Plan p = Resolve(row, layout, Point{750, 795}, 3, none);
    Check(p.zone == Zone::kRootEdge && p.direction == Direction::kDown,
          "outer bottom band -> root edge");
    Check(Is(p.preview, 0, 400, 1200, 400), "preview = the whole bottom half");
  }
  // Dragged "far enough": the outer band is 8% of the split's height (64 of
  // 800), so 49 DIP above the bottom already spans the whole bottom.
  {
    const Plan p = Resolve(row, layout, Point{750, 750}, 3, none);
    Check(p.zone == Zone::kRootEdge && p.direction == Direction::kDown,
          "deep toward the bottom -> root edge");
  }
  // A pane's centre swaps: the preview is that pane's own rect.
  {
    const Plan p = Resolve(row, layout, Point{750, 400}, 3, none);
    Check(p.zone == Zone::kSwap && p.target == 2, "centre of pane 2 -> swap");
    Check(Is(p.preview, 600, 0, 300, 800), "swap preview = pane 2's rect");
  }
  // Diagonals pick the nearest edge: near pane 2's left side -> left of 2.
  {
    const Plan p = Resolve(row, layout, Point{610, 420}, 3, none);
    Check(p.zone == Zone::kPaneEdge && p.target == 2 &&
              p.direction == Direction::kLeft,
          "left strip of pane 2 -> left of 2");
  }
  // Nothing over the dragged pane itself, nor far outside the split.
  {
    Check(Resolve(row, layout, Point{1050, 400}, 3, none).zone == Zone::kNone,
          "over the dragged pane -> nothing");
    Check(Resolve(row, layout, Point{600, 900}, 3, none).zone == Zone::kNone,
          "100 DIP below the split -> nothing");
  }
  // A slight overshoot is clamped in and still hits the outer band.
  {
    const Plan p = Resolve(row, layout, Point{600, 820}, 3, none);
    Check(p.zone == Zone::kRootEdge && p.direction == Direction::kDown,
          "20 DIP overshoot -> root bottom");
  }
  // A no-op outer edge (pane 3 already spans the right edge) falls through to
  // the pane under the pointer.
  {
    const Plan p = Resolve(row, layout, Point{1195, 400}, 0, none);
    Check(p.zone == Zone::kRootEdge && p.direction == Direction::kRight,
          "dragging pane 0 to the right band -> root right (a reorder)");
    const Plan q = Resolve(row, layout, Point{895, 300}, 3, none);
    Check(q.zone == Zone::kNone, "right of 2 is where 3 already is -> nothing");
  }
  // Hysteresis: an active outer band holds 32 DIP past its 64 DIP depth...
  {
    const Plan shown = Resolve(row, layout, Point{750, 795}, 3, none);
    const Plan held = Resolve(row, layout, Point{750, 720}, 3, shown);
    Check(held.zone == Zone::kRootEdge, "outer band holds at 79 DIP");
    const Plan released = Resolve(row, layout, Point{750, 690}, 3, shown);
    Check(released.zone == Zone::kPaneEdge, "outer band releases past 96 DIP");
  }
  // ...and an active pane edge holds into the centre box (no flip to swap).
  {
    const Plan shown = Resolve(row, layout, Point{750, 700}, 3, none);
    const Plan held =
        Resolve(row, layout, Point{750, 520}, 3, shown);  // v = 0.65
    Check(held.zone == Zone::kPaneEdge && held.direction == Direction::kDown,
          "pane edge holds at 65%");
    const Plan fresh = Resolve(row, layout, Point{750, 520}, 3, none);
    Check(fresh.zone == Zone::kSwap, "same spot without a held edge -> swap");
  }
  // The returned tree is exactly what the drop commits.
  {
    const Plan p = Resolve(row, layout, Point{750, 700}, 3, none);
    std::vector<int> order;
    CollectLeaves(p.result, order);
    Check(order == std::vector<int>({0, 1, 2, 3}), "result keeps every pane");
    Check(!p.result.isLeaf && p.result.children.size() == 3,
          "result is a row of three");
  }

  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
