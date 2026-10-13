// Unit tests for the pure split-tree model (pane_tree.h). No Edge/Win32 deps, so
// this builds and runs on the host. Run via test\run.cmd.
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "../src/pane_tree.h"

using namespace fixedge;

namespace {

int g_checks = 0;
void Check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    std::exit(1);
  }
}

// Render leaf order + each leaf's rect as a string, for stable assertions.
std::string Layout(const Node& root, int w, int h) {
  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, w, h}, 0, leaves);
  std::string s;
  for (const LeafRect& lr : leaves) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d:[%d,%d,%d,%d] ", lr.leaf, lr.rect.x, lr.rect.y,
                  lr.rect.w, lr.rect.h);
    s += buf;
  }
  return s;
}

std::vector<int> Order(const Node& root) {
  std::vector<int> v;
  CollectLeaves(root, v);
  return v;
}

}  // namespace

int main() {
  // Flat equal columns tile the width, last absorbs rounding, full height.
  {
    Node t = FlatTree(3, Axis::kHorizontal);
    Check(Layout(t, 900, 600) == "0:[0,0,300,600] 1:[300,0,300,600] 2:[600,0,300,600] ",
          "flat 3 equal columns");
  }
  // 10 into 3: 3,3,4 (remainder to the last) and they exactly tile 10 wide.
  {
    Node t = FlatTree(3, Axis::kHorizontal);
    std::vector<LeafRect> leaves;
    LayoutTree(t, Rect{0, 0, 10, 2}, 0, leaves);
    Check(leaves[0].rect.w + leaves[1].rect.w + leaves[2].rect.w == 10, "columns tile exactly");
    Check(leaves[2].rect.x + leaves[2].rect.w == 10, "last column reaches the edge");
  }
  // Vertical flat tree stacks top->bottom, full width.
  {
    Node t = FlatTree(2, Axis::kVertical);
    Check(Layout(t, 400, 300) == "0:[0,0,400,150] 1:[0,150,400,150] ", "flat 2 rows");
  }

  // Directional add: start with one horizontal row {0,1}; add pane 2 BELOW pane 1
  // -> pane 1 becomes a vertical split {1 over 2}; order is 0,1,2.
  {
    Node t = FlatTree(2, Axis::kHorizontal);
    Check(SplitPane(t, 1, Direction::kDown, 2), "split pane 1 down found target");
    Check(Order(t) == std::vector<int>({0, 1, 2}), "order after down-split");
    // Geometry: left half pane 0 full height; right half split into 1 (top) / 2 (bottom).
    Check(Layout(t, 400, 200) ==
              "0:[0,0,200,200] 1:[200,0,200,100] 2:[200,100,200,100] ",
          "down-split geometry");
  }
  // Add LEFT of pane 0 in a horizontal row joins the same row before it.
  {
    Node t = FlatTree(2, Axis::kHorizontal);  // {0,1}
    Check(SplitPane(t, 0, Direction::kLeft, 2), "split pane 0 left");
    Check(Order(t) == std::vector<int>({2, 0, 1}), "new pane inserted before on same axis");
  }
  // Add RIGHT of pane 0 in a horizontal row inserts right after it.
  {
    Node t = FlatTree(2, Axis::kHorizontal);  // {0,1}
    Check(SplitPane(t, 0, Direction::kRight, 2), "split pane 0 right");
    Check(Order(t) == std::vector<int>({0, 2, 1}), "new pane inserted after on same axis");
  }

  // Swap axis: in a horizontal row, a pane's swap faces left/right; after a
  // down-split, pane 1's swap faces up/down (its parent is vertical).
  {
    Node t = FlatTree(2, Axis::kHorizontal);
    Check(SwapAxisOf(t, 0) == Axis::kHorizontal, "row pane swaps horizontally");
    SplitPane(t, 1, Direction::kDown, 2);
    Check(SwapAxisOf(t, 1) == Axis::kVertical, "stacked pane swaps vertically");
    Check(SwapAxisOf(t, 0) == Axis::kHorizontal, "outer pane still horizontal");
  }
  // Swap with counterpart exchanges adjacent siblings.
  {
    Node t = FlatTree(3, Axis::kHorizontal);  // 0,1,2
    Check(SwapWithCounterpart(t, 0), "swap pane 0");
    Check(Order(t) == std::vector<int>({1, 0, 2}), "0 swapped with next sibling");
  }

  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
