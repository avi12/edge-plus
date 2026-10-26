// Unit tests for tab_grid.h.
#include <cstdio>
#include <vector>
#include "../../src/layout/tab_grid.h"

using namespace edgeplus;
using namespace edgeplus::tab_grid;

namespace {

struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

using Sizes = std::vector<int>;

void RunChecks() {
  {
    const TabGrid grid = GridOfTree(FlatTree(3, Axis::kHorizontal));
    Check(grid.rowSizes == Sizes({3}), "3 panes side by side are one row of 3");
    Check(grid.order == Sizes({0, 1, 2}), "left to right");
  }
  {
    const TabGrid grid = GridOfTree(FlatTree(3, Axis::kVertical));
    Check(grid.rowSizes == Sizes({1, 1, 1}), "3 stacked panes are 3 rows");
  }
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    SplitPane(tree, 0, Direction::kDown, 2);
    SplitPane(tree, 1, Direction::kDown, 3);  // H[V[0 2] V[1 3]]
    const TabGrid grid = GridOfTree(tree);
    Check(grid.rowSizes == Sizes({2, 2}), "a 2x2 split is two rows of 2");
    Check(grid.order == Sizes({0, 1, 2, 3}), "2x2 reads across each row");
  }
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    SplitPane(tree, 1, Direction::kDown, 2);  // H[0 V[1 2]]
    const TabGrid grid = GridOfTree(tree);
    Check(grid.rowSizes == Sizes({2, 1}), "one tall pane beside two stacked");
    Check(grid.order == Sizes({0, 1, 2}), "tall pane first, then the stack");
  }
  Check(GridOfFlatSplit(3, 600).rowSizes == Sizes({3}),
        "a flat split of 3 fits one row");
  Check(GridOfFlatSplit(3, 250).rowSizes == Sizes({2, 1}),
        "a flat split wraps when tabs would be narrower than the minimum");
  Check(GridOfFlatSplit(5, 350).rowSizes == Sizes({3, 2}),
        "wrapped rows share the tabs out, earlier rows taking more");
  Check(GridOfFlatSplit(3, 40).rowSizes == Sizes({1, 1, 1}),
        "a very narrow list gets one tab per row");
  Check(GridOfFlatSplit(3, 600).order.empty(), "a flat split keeps strip order");
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
