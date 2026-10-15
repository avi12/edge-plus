// Unit tests for the drop-zone resolver (dock.h). No Edge/Win32 deps. Run via
// test\run.cmd.
#include <cstdio>
#include <string>
#include <vector>
#include "../src/dock.h"

using namespace fixedge;
using namespace fixedge::dock;

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

bool HasBounds(const Rect& rect, int x, int y, int width, int height) {
  return rect.x == x && rect.y == y && rect.width == width &&
         rect.height == height;
}

void RunChecks() {
  // A 1216x808 split holding one row of four 298-wide panes, kPaneGap (8)
  // apart: x = 0, 306, 612, 918. Three columns of it are 400 wide (x = 0, 408,
  // 816) and two rows 400 tall (y = 0, 408). We drag pane 3.
  const Rect layout{0, 0, 1216, 808};
  const Node row = FlatTree(4, Axis::kHorizontal);
  const Plan none;

  // Scenario (a): the bottom of pane 2 (above the outer band) docks below it;
  // the preview is pane 3's final rect: the bottom half of a third column.
  {
    const Plan plan = Resolve(row, layout, Point{750, 700}, 3, none);
    Check(plan.zone == Zone::kPaneEdge && plan.target == 2 &&
              plan.direction == Direction::kDown,
          "bottom of pane 2 -> below 2");
    Check(HasBounds(plan.preview, 816, 408, 400, 400),
          "preview = bottom half of the third column");
  }
  // Scenario (b): within the outer band at the bottom -> the full bottom half.
  {
    const Plan plan = Resolve(row, layout, Point{750, 795}, 3, none);
    Check(plan.zone == Zone::kRootEdge && plan.direction == Direction::kDown,
          "outer bottom band -> root edge");
    Check(HasBounds(plan.preview, 0, 408, 1216, 400),
          "preview = the whole bottom half");
  }
  // Dragged "far enough": the outer band is 8% of the split's height (64 of
  // 808), so 57 DIP above the bottom already spans the whole bottom.
  {
    const Plan plan = Resolve(row, layout, Point{750, 750}, 3, none);
    Check(plan.zone == Zone::kRootEdge && plan.direction == Direction::kDown,
          "deep toward the bottom -> root edge");
  }
  // A pane's centre swaps: the preview is that pane's own rect.
  {
    const Plan plan = Resolve(row, layout, Point{750, 400}, 3, none);
    Check(plan.zone == Zone::kSwap && plan.target == 2,
          "centre of pane 2 -> swap");
    Check(HasBounds(plan.preview, 612, 0, 298, 808),
          "swap preview = pane 2's rect");
  }
  // Diagonals pick the nearest edge: near pane 2's left side -> left of 2.
  {
    const Plan plan = Resolve(row, layout, Point{622, 420}, 3, none);
    Check(plan.zone == Zone::kPaneEdge && plan.target == 2 &&
              plan.direction == Direction::kLeft,
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
    const Plan plan = Resolve(row, layout, Point{600, 828}, 3, none);
    Check(plan.zone == Zone::kRootEdge && plan.direction == Direction::kDown,
          "20 DIP overshoot -> root bottom");
  }
  // A no-op outer edge (pane 3 already spans the right edge) falls through to
  // the pane under the pointer.
  {
    const Plan reorder = Resolve(row, layout, Point{1195, 400}, 0, none);
    Check(reorder.zone == Zone::kRootEdge &&
              reorder.direction == Direction::kRight,
          "dragging pane 0 to the right band -> root right (a reorder)");
    const Plan noOp = Resolve(row, layout, Point{895, 300}, 3, none);
    Check(noOp.zone == Zone::kNone,
          "right of 2 is where 3 already is -> nothing");
  }
  // Hysteresis: an active outer band holds 32 DIP past its 64 DIP depth...
  {
    const Plan shown = Resolve(row, layout, Point{750, 795}, 3, none);
    const Plan held = Resolve(row, layout, Point{750, 728}, 3, shown);
    Check(held.zone == Zone::kRootEdge, "outer band holds at 79 DIP");
    const Plan released = Resolve(row, layout, Point{750, 698}, 3, shown);
    Check(released.zone == Zone::kPaneEdge, "outer band releases past 96 DIP");
  }
  // ...and an active pane edge holds into the centre box (no flip to swap).
  {
    const Plan shown = Resolve(row, layout, Point{750, 700}, 3, none);
    const Plan held =
        Resolve(row, layout, Point{750, 525}, 3, shown);  // paneY = 0.65
    Check(held.zone == Zone::kPaneEdge && held.direction == Direction::kDown,
          "pane edge holds at 65%");
    const Plan fresh = Resolve(row, layout, Point{750, 525}, 3, none);
    Check(fresh.zone == Zone::kSwap, "same spot without a held edge -> swap");
  }
  // An incoming pane (a tab dragged in) joins the tree: three panes, the new
  // one is leaf 3.
  {
    const Node three = FlatTree(3, Axis::kHorizontal);
    const Plan below = ResolveIncoming(three, layout, Point{600, 700}, 3, none);
    Check(below.zone == Zone::kPaneEdge && below.target == 1 &&
              below.direction == Direction::kDown,
          "incoming: bottom of pane 1 -> below 1");
    Check(HasBounds(below.preview, 408, 408, 400, 400),
          "incoming preview = bottom half of the middle column");
    std::vector<int> order;
    CollectLeaves(below.result, order);
    Check(order == std::vector<int>({0, 1, 3, 2}), "incoming joins the tree");
    const Plan span = ResolveIncoming(three, layout, Point{600, 795}, 3, none);
    Check(span.zone == Zone::kRootEdge && span.direction == Direction::kDown &&
              HasBounds(span.preview, 0, 408, 1216, 400),
          "incoming: outer bottom band spans the bottom");
    const Plan centre =
        ResolveIncoming(three, layout, Point{600, 380}, 3, none);
    Check(centre.zone == Zone::kPaneEdge && centre.target == 1,
          "incoming never swaps: a pane's centre docks beside it");
    Check(ResolveIncoming(three, layout, Point{600, -10}, 3, none).zone ==
              Zone::kNone,
          "incoming: just above the split (the strip) -> nothing");
  }
  // The gap between panes 1 and 2 is neither's: a zone already shown holds
  // there, and with none shown there is nothing to drop on.
  {
    const Point inGap{608, 400};
    Check(Resolve(row, layout, inGap, 3, none).zone == Zone::kNone,
          "the gap alone -> nothing");
    const Plan shown = Resolve(row, layout, Point{750, 700}, 3, none);
    const Plan held = Resolve(row, layout, inGap, 3, shown);
    Check(held.zone == Zone::kPaneEdge && held.target == 2 &&
              held.direction == Direction::kDown,
          "crossing the gap keeps below 2");
  }
  // The returned tree is exactly what the drop commits.
  {
    const Plan plan = Resolve(row, layout, Point{750, 700}, 3, none);
    std::vector<int> order;
    CollectLeaves(plan.result, order);
    Check(order == std::vector<int>({0, 1, 2, 3}), "result keeps every pane");
    Check(!plan.result.isLeaf && plan.result.children.size() == 3,
          "result is a row of three");
  }
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
