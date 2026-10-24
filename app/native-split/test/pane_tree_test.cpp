// Unit tests for pane_tree.h.
#include <cstdio>
#include <string>
#include <vector>
#include "../src/pane_tree.h"

using namespace fixedge;

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

std::string Layout(const Node& root, int width, int height) {
  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, width, height}, 0, leaves);
  std::string text;
  for (const LeafRect& leafRect : leaves) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d:[%d,%d,%d,%d] ", leafRect.leaf,
                  leafRect.rect.x, leafRect.rect.y, leafRect.rect.width,
                  leafRect.rect.height);
    text += buffer;
  }
  return text;
}

std::vector<int> Order(const Node& root) {
  std::vector<int> order;
  CollectLeaves(root, order);
  return order;
}

void RunChecks() {
  // Flat equal columns tile the width, last absorbs rounding, full height.
  {
    const Node tree = FlatTree(3, Axis::kHorizontal);
    Check(Layout(tree, 900, 600) ==
              "0:[0,0,300,600] 1:[300,0,300,600] 2:[600,0,300,600] ",
          "flat 3 equal columns");
  }
  // 10 into 3: 3,3,4 (remainder to the last) and they exactly tile 10 wide.
  {
    const Node tree = FlatTree(3, Axis::kHorizontal);
    std::vector<LeafRect> leaves;
    LayoutTree(tree, Rect{0, 0, 10, 2}, 0, leaves);
    Check(leaves[0].rect.width + leaves[1].rect.width + leaves[2].rect.width ==
              10,
          "columns tile exactly");
    Check(leaves[2].rect.x + leaves[2].rect.width == 10,
          "last column reaches the edge");
  }
  // Vertical flat tree stacks top->bottom, full width.
  {
    const Node tree = FlatTree(2, Axis::kVertical);
    Check(Layout(tree, 400, 300) == "0:[0,0,400,150] 1:[0,150,400,150] ",
          "flat 2 rows");
  }

  // Directional add: start with one horizontal row {0,1}; add pane 2 BELOW pane 1
  // -> pane 1 becomes a vertical split {1 over 2}; order is 0,1,2.
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    Check(SplitPane(tree, 1, Direction::kDown, 2),
          "split pane 1 down found target");
    Check(Order(tree) == std::vector<int>({0, 1, 2}), "order after down-split");
    // Geometry: left half pane 0 full height; right half split into 1 (top) / 2 (bottom).
    Check(Layout(tree, 400, 200) ==
              "0:[0,0,200,200] 1:[200,0,200,100] 2:[200,100,200,100] ",
          "down-split geometry");
  }
  // Add LEFT of pane 0 in a horizontal row joins the same row before it.
  {
    Node tree = FlatTree(2, Axis::kHorizontal);  // {0,1}
    Check(SplitPane(tree, 0, Direction::kLeft, 2), "split pane 0 left");
    Check(Order(tree) == std::vector<int>({2, 0, 1}),
          "new pane inserted before on same axis");
  }
  // Add RIGHT of pane 0 in a horizontal row inserts right after it.
  {
    Node tree = FlatTree(2, Axis::kHorizontal);  // {0,1}
    Check(SplitPane(tree, 0, Direction::kRight, 2), "split pane 0 right");
    Check(Order(tree) == std::vector<int>({0, 2, 1}),
          "new pane inserted after on same axis");
  }

  // Swap axis: in a horizontal row, a pane's swap faces left/right; after a
  // down-split, pane 1's swap faces up/down (its parent is vertical).
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    Check(SwapAxisOf(tree, 0) == Axis::kHorizontal,
          "row pane swaps horizontally");
    SplitPane(tree, 1, Direction::kDown, 2);
    Check(SwapAxisOf(tree, 1) == Axis::kVertical,
          "stacked pane swaps vertically");
    Check(SwapAxisOf(tree, 0) == Axis::kHorizontal,
          "outer pane still horizontal");
  }
  // Swap with counterpart exchanges adjacent siblings.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);  // 0,1,2
    Check(SwapWithCounterpart(tree, 0), "swap pane 0");
    Check(Order(tree) == std::vector<int>({1, 0, 2}),
          "0 swapped with next sibling");
  }

  // Resize reflows proportionally: a 1:1 row stays 1:1 as the window widens, and
  // a weighted split keeps its ratio across sizes.
  {
    const Node tree = FlatTree(2, Axis::kHorizontal);
    std::vector<LeafRect> narrow;
    std::vector<LeafRect> wide;
    LayoutTree(tree, Rect{0, 0, 400, 200}, 0, narrow);
    LayoutTree(tree, Rect{0, 0, 800, 200}, 0, wide);
    Check(narrow[0].rect.width == 200 && narrow[1].rect.width == 200,
          "400 wide splits 200/200");
    Check(wide[0].rect.width == 400 && wide[1].rect.width == 400,
          "800 wide splits 400/400 (reflow)");
    Check(wide[1].rect.x == 400 && wide[1].rect.x + wide[1].rect.width == 800,
          "reflow keeps panes seamless");
  }
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    tree.weights = {3.f, 1.f};  // 3:1
    std::vector<LeafRect> narrow;
    std::vector<LeafRect> wide;
    LayoutTree(tree, Rect{0, 0, 400, 100}, 0, narrow);
    LayoutTree(tree, Rect{0, 0, 1200, 100}, 0, wide);
    Check(narrow[0].rect.width == 300 && narrow[1].rect.width == 100,
          "3:1 ratio at 400");
    Check(wide[0].rect.width == 900 && wide[1].rect.width == 300,
          "3:1 ratio preserved at 1200");
  }
  // Leaf count: what LiveTree compares against the panes it must tile.
  {
    Check(LeafCount(MakeLeaf(0)) == 1, "a leaf tiles one pane");
    Check(LeafCount(FlatTree(4, Axis::kHorizontal)) == 4,
          "a flat split tiles its count");
    Node tree = FlatTree(2, Axis::kHorizontal);
    SplitPane(tree, 1, Direction::kDown, 2);
    Check(LeafCount(tree) == 3, "a nested split counts every leaf");
  }
  // Minimum pane size: a tiny window never collapses a pane below span/n, and
  // panes still tile exactly (no negative widths, no overflow).
  {
    const Node tree = FlatTree(3, Axis::kHorizontal);
    std::vector<LeafRect> leaves;
    LayoutTree(tree, Rect{0, 0, 30, 50}, 0, leaves);
    int sum = 0;
    for (const LeafRect& leafRect : leaves) {
      Check(leafRect.rect.width >= 10, "no pane below span/n on a tiny window");
      Check(leafRect.rect.width > 0, "no zero/negative pane");
      sum += leafRect.rect.width;
    }
    Check(sum == 30, "panes still tile the tiny window exactly");
  }
  // A pane that left the split and came back as its newest pane keeps its
  // place: H[0 V[1 2] 3] with pane 1 back as pane 3 is H[0 V[3 1] 2].
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    SplitPane(tree, 1, Direction::kDown, 3);
    std::swap(tree.children[1].children[1].leaf,
              tree.children[2].leaf);  // H[0 V[1 2] 3]
    Check(Order(tree) == std::vector<int>{0, 1, 2, 3}, "the starting tree");
    const std::string before = Layout(tree, 900, 600);
    RenumberAsNewest(tree, 1, 3);
    Check(Order(tree) == std::vector<int>{0, 3, 1, 2},
          "the returning pane is the newest, the later ones one earlier");
    Check(Layout(tree, 900, 600) ==
              "0:[0,0,300,600] 3:[300,0,300,300] 1:[300,300,300,300] "
              "2:[600,0,300,600] ",
          "every pane keeps its rect");
    Check(before ==
              "0:[0,0,300,600] 1:[300,0,300,300] 2:[300,300,300,300] "
              "3:[600,0,300,600] ",
          "the rects it started from");
  }
  {
    Node tree = MakeLeaf(0);
    SplitPane(tree, 0, Direction::kUp, 1);
    Pair pair;
    Check(ReadPair(tree, pair), "a pane added above makes a pair");
    Check(pair.axis == Axis::kVertical && pair.isSecondLeafFirst,
          "the new pane on top, one above the other");
  }
  {
    Node tree = MakeLeaf(0);
    SplitPane(tree, 0, Direction::kRight, 1);
    Pair pair;
    Check(ReadPair(tree, pair), "a pane added right makes a pair");
    Check(pair.axis == Axis::kHorizontal && !pair.isSecondLeafFirst,
          "the new pane second, side by side");
  }
  {
    const Node tree = FlatTree(3, Axis::kHorizontal);
    Pair pair;
    Check(!ReadPair(tree, pair), "three panes are no pair");
  }
  {
    Node columns = FlatTree(2, Axis::kHorizontal);
    SplitPane(columns, 0, Direction::kDown, 2);
    SplitPane(columns, 1, Direction::kDown, 3);  // H[V[0 2] V[1 3]]
    Check(Order(columns) == std::vector<int>{0, 2, 1, 3},
          "a grid built as columns reads down each column in the tree");
    Check(ReadingOrder(columns) == std::vector<int>{0, 1, 2, 3},
          "on screen it reads across each row");
  }
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    SplitPane(tree, 0, Direction::kUp, 2);  // H[V[2 0] 1]
    Check(ReadingOrder(tree) == std::vector<int>{2, 1, 0},
          "a pane added above comes first, beside the tall one");
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
