// Unit tests for pane_edits.h.
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "../../src/layout/pane_edits.h"
#include "../../src/layout/pane_layout.h"
#include "../../src/layout/pane_moves.h"
#include "../../src/layout/pane_normalize.h"
#include "../../src/layout/pane_tree.h"

using namespace edgeplus;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what, const std::string& got = "") {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s %s\n", what, got.c_str());
    throw CheckFailed{};
  }
}

// H[0(1) 1(1) V[2(1) 3(1)](1)] — axis, leaves and slot weights.
std::string Dump(const Node& node) {
  if (node.isLeaf) {
    return std::to_string(node.leaf);
  }
  std::string text = node.axis == Axis::kHorizontal ? "H[" : "V[";
  for (size_t i = 0; i < node.children.size(); i++) {
    char weightText[32];
    std::snprintf(weightText, sizeof(weightText), "(%.3g)", node.weights[i]);
    const bool hasPreviousChild = i > 0;
    if (hasPreviousChild) {
      text += " ";
    }
    text += Dump(node.children[i]) + weightText;
  }
  return text + "]";
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

Node Weighted(int count, Axis axis, std::vector<float> weights) {
  Node tree = FlatTree(count, axis);
  tree.weights = std::move(weights);
  FixWeights(tree);
  return tree;
}

void RunChecks() {
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

  // Closing the bottom pane of V[H[0 1 2] 3]: the top row takes the full
  // height.
  {
    Node tree = FlatTree(4, Axis::kHorizontal);
    MovePaneToRootEdge(tree, 3, Direction::kDown);
    Check(RemovePane(tree, 3), "close bottom pane");
    Check(Dump(tree) == "H[0(1) 1(1) 2(1)]", "top row fills the split",
          Dump(tree));
  }

  // Closing the top-right pane: the other two top panes split the row in
  // halves, the bottom pane is untouched, and the bottom pane's id (3) closes
  // the gap (2) because its container is re-fed one slot earlier.
  {
    Node tree = FlatTree(4, Axis::kHorizontal);
    MovePaneToRootEdge(tree, 3, Direction::kDown);
    Check(RemovePane(tree, 2), "close top-right pane");
    Check(Dump(tree) == "V[H[0(1) 1(1)](1) 2(1)]",
          "top halves, bottom unchanged", Dump(tree));
    Check(Layout(tree, 1200, 800) ==
              "0:[0,0,600,400] 1:[600,0,600,400] 2:[0,400,1200,400] ",
          "close top-right geometry", Layout(tree, 1200, 800));
  }

  // Closing a pane keeps the proportions the others were resized to.
  {
    Node tree = Weighted(3, Axis::kHorizontal, {2.f, 1.f, 1.f});
    Check(RemovePane(tree, 2), "close in a resized row");
    Check(Dump(tree) == "H[0(1.33) 1(0.667)]", "resized ratio 2:1 kept",
          Dump(tree));
    Node single = MakeLeaf(0);
    Check(!RemovePane(single, 0), "the last pane is never removed");
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
