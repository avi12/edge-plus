// Unit tests for the drag-and-drop tree edits in pane_tree.h (MovePane,
// MovePaneToRootEdge, SwapLeaves, Normalize). No Edge/Win32 deps. Run via
// test\run.cmd.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include "../src/pane_tree.h"

using namespace fixedge;

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

// How far a split's weights may sum from its child count and still be
// canonical: float rounding, not a real drift.
constexpr float kWeightSumTolerance = 1e-3f;

// The canonical form every edit must leave behind.
bool Canonical(const Node& node) {
  if (node.isLeaf) {
    return true;
  }
  const bool hasSplitShape =
      node.children.size() >= 2 && node.weights.size() == node.children.size();
  if (!hasSplitShape) {
    return false;
  }
  float sum = 0.f;
  for (const float weight : node.weights) {
    const bool isPositive = weight > 0.f;
    if (!isPositive) {
      return false;
    }
    sum += weight;
  }
  const bool hasSumDrifted =
      std::fabs(sum - static_cast<float>(node.children.size())) >
      kWeightSumTolerance;
  if (hasSumDrifted) {
    return false;
  }
  for (const Node& child : node.children) {
    const bool isSameAxisSplit = !child.isLeaf && child.axis == node.axis;
    if (isSameAxisSplit) {
      return false;
    }
    if (!Canonical(child)) {
      return false;
    }
  }
  return true;
}

Node Weighted(int count, Axis axis, std::vector<float> weights) {
  Node tree = FlatTree(count, axis);
  tree.weights = std::move(weights);
  FixWeights(tree);
  return tree;
}

// The random-edit run. The seed is fixed so a failure reproduces.
constexpr unsigned kRandomSeed = 12345;
constexpr int kRandomRuns = 200;
constexpr int kEditsPerRun = 60;
constexpr int kMaxPanes = 9;  // a split stops adding panes here

// The edits a random step can make, drawn uniformly.
enum class Edit : uint8_t {
  kSplit,
  kMove,
  kMoveToRootEdge,
  kSwap
};
constexpr unsigned kEditCount = 4;
constexpr unsigned kDirectionCount = 4;

void RunChecks() {
  // The user's scenario (a): row 1..4, drop 4 on the bottom edge of 3 -> thirds
  // across, and 3 / 4 share the third column at equal heights.
  {
    Node tree = FlatTree(4, Axis::kHorizontal);
    Check(MovePane(tree, 3, 2, Direction::kDown), "drop below 3 moved");
    Check(Dump(tree) == "H[0(1) 1(1) V[2(1) 3(1)](1)]", "drop below 3 tree",
          Dump(tree));
    Check(Layout(tree, 1200, 800) ==
              "0:[0,0,400,800] 1:[400,0,400,800] 2:[800,0,400,400] "
              "3:[800,400,400,400] ",
          "drop below 3 geometry", Layout(tree, 1200, 800));
  }
  // The user's scenario (b): drop 4 on the bottom edge of the whole layout -> it
  // takes the full bottom half; 1..3 share the top half in thirds.
  {
    Node tree = FlatTree(4, Axis::kHorizontal);
    Check(MovePaneToRootEdge(tree, 3, Direction::kDown), "root bottom moved");
    Check(Dump(tree) == "V[H[0(1) 1(1) 2(1)](1) 3(1)]", "root bottom tree",
          Dump(tree));
    Check(Layout(tree, 1200, 800) ==
              "0:[0,0,400,400] 1:[400,0,400,400] 2:[800,0,400,400] "
              "3:[0,400,1200,400] ",
          "root bottom geometry", Layout(tree, 1200, 800));
  }
  // No-ops leave the tree untouched.
  {
    Node tree = FlatTree(4, Axis::kHorizontal);
    const std::string before = Dump(tree);
    Check(!MovePane(tree, 2, 2, Direction::kDown), "drop on itself");
    Check(!MovePane(tree, 3, 2, Direction::kRight), "already right of target");
    Check(!MovePane(tree, 2, 3, Direction::kLeft), "already left of target");
    Check(!MovePaneToRootEdge(tree, 3, Direction::kRight),
          "already spans right edge");
    Check(!MovePaneToRootEdge(tree, 0, Direction::kLeft),
          "already spans left edge");
    Check(!MovePane(tree, 9, 2, Direction::kDown), "missing moving pane");
    Check(!MovePane(tree, 2, 9, Direction::kDown), "missing target pane");
    Check(!SwapLeaves(tree, 1, 1), "swap with itself");
    Check(!SwapLeaves(tree, 1, 9), "swap with missing");
    Check(Dump(tree) == before, "no-ops leave the tree unchanged", Dump(tree));
    Node single = MakeLeaf(0);
    Check(!MovePaneToRootEdge(single, 0, Direction::kDown),
          "single pane has no root edge");
    Node column = Weighted(2, Axis::kVertical, {3.f, 1.f});
    Check(!MovePane(column, 1, 0, Direction::kDown),
          "already below -> keeps 3:1");
  }
  // A reorder within one split carries the pane's own size.
  {
    Node tree = Weighted(3, Axis::kHorizontal, {3.f, 1.f, 1.f});
    Check(MovePane(tree, 0, 2, Direction::kRight), "reorder moved");
    Check(Dump(tree) == "H[1(0.6) 2(0.6) 0(1.8)]", "reorder keeps weight",
          Dump(tree));
  }
  // A 2-pane column turned into a row (the emptied split collapses).
  {
    Node tree = FlatTree(2, Axis::kVertical);
    Check(MovePane(tree, 1, 0, Direction::kRight), "pair transpose moved");
    Check(Dump(tree) == "H[0(1) 1(1)]", "V[0,1] -> H[0,1]", Dump(tree));
  }
  // Removing a pane flattens what is left without changing on-screen widths.
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    SplitPane(tree, 1, Direction::kDown, 2);   // H[0, V[1,2]]
    SplitPane(tree, 2, Direction::kRight, 3);  // H[0, V[1, H[2,3]]]
    Check(Canonical(tree), "setup canonical", Dump(tree));
    Check(MovePaneToRootEdge(tree, 1, Direction::kDown), "flatten move");
    Check(Dump(tree) == "V[H[0(1.5) 2(0.75) 3(0.75)](1) 1(1)]",
          "flatten keeps widths", Dump(tree));
    Check(Layout(tree, 800, 400) ==
              "0:[0,0,400,200] 2:[400,0,200,200] 3:[600,0,200,200] "
              "1:[0,200,800,200] ",
          "flatten geometry", Layout(tree, 800, 400));
  }
  // Removal shares the freed space in proportion (a 2:1 pair stays 2:1).
  {
    Node tree = Weighted(3, Axis::kHorizontal, {2.f, 1.f, 1.f});
    MovePaneToRootEdge(tree, 2, Direction::kDown);
    Check(Dump(tree) == "V[H[0(1.33) 1(0.667)](1) 2(1)]",
          "proportional removal", Dump(tree));
  }
  // Joining a resized row only halves the target; an equal row stays equal.
  {
    Node resized = Weighted(2, Axis::kHorizontal, {3.f, 1.f});
    SplitPane(resized, 1, Direction::kDown, 2);
    MovePane(resized, 2, 1, Direction::kRight);
    Check(Dump(resized) == "H[0(2.25) 1(0.375) 2(0.375)]",
          "resized row: half of target", Dump(resized));
    Node equal = FlatTree(3, Axis::kHorizontal);
    SplitPane(equal, 2, Direction::kDown, 3);
    MovePane(equal, 3, 1, Direction::kRight);
    Check(Dump(equal) == "H[0(1) 1(1) 3(1) 2(1)]", "equal row: quarters",
          Dump(equal));
  }
  // A root along the drop axis gains the pane as an average-share end child.
  {
    Node tree = FlatTree(2, Axis::kHorizontal);
    MovePaneToRootEdge(tree, 1, Direction::kDown);  // V[0,1]
    SplitPane(tree, 0, Direction::kRight, 2);       // V[H[0,2],1]
    MovePaneToRootEdge(tree, 0, Direction::kDown);  // V[2,1,0]
    Check(Dump(tree) == "V[2(1) 1(1) 0(1)]", "root join -> thirds", Dump(tree));
  }
  // Centre drop: positions keep their sizes, the panes trade places.
  {
    Node tree = Weighted(3, Axis::kHorizontal, {2.f, 1.f, 1.f});
    Check(SwapLeaves(tree, 0, 2), "swap moved");
    Check(Dump(tree) == "H[2(1.5) 1(0.75) 0(0.75)]",
          "swap keeps sizes with positions", Dump(tree));
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
  // "Use equal widths / heights" evens out only the nearest split along that
  // axis around the pane.
  {
    Node tree = Weighted(3, Axis::kHorizontal, {3.f, 1.f, 1.f});
    SplitPane(tree, 2, Direction::kDown, 3);  // H[0 1 V[2 3]]
    NearestSplitAlong(tree, 2, Axis::kVertical)->weights = {1.5f, 0.5f};
    Check(EqualizeAlong(tree, 3, Axis::kHorizontal), "equal widths");
    Check(Dump(tree) == "H[0(1) 1(1) V[2(1.5) 3(0.5)](1)]",
          "widths even, the column's heights untouched", Dump(tree));
    Check(EqualizeAlong(tree, 3, Axis::kVertical), "equal heights");
    Check(Dump(tree) == "H[0(1) 1(1) V[2(1) 3(1)](1)]", "heights even",
          Dump(tree));
    Check(!EqualizeAlong(tree, 0, Axis::kVertical),
          "no vertical split around pane 0");
    Check(!EqualizeAlong(tree, 0, Axis::kHorizontal), "already even");
  }
  // Seams: a tree of N panes has N - 1, each between two neighbours.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    SplitPane(tree, 2, Direction::kDown, 3);  // H[0 1 V[2 3]]
    const std::vector<Seam> seams = Seams(tree, Rect{0, 0, 900, 600});
    Check(seams.size() == 3, "four panes, three seams");
    Check(seams[0].axis == Axis::kHorizontal && seams[0].position == 300 &&
              seams[0].crossLength == 600,
          "seam between 0 and 1: x=300, full height");
    Check(seams[2].axis == Axis::kVertical && seams[2].position == 300 &&
              seams[2].crossStart == 600 && seams[2].crossLength == 300,
          "seam between 2 and 3: y=300, the third column's width");
  }
  // Dragging a seam trades length between its two panes only.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    const Seam seam = Seams(tree, Rect{0, 0, 900, 600})[0];
    Check(ResizeSeam(tree, seam, 60), "drag seam 0 by +60");
    Check(Layout(tree, 900, 600) ==
              "0:[0,0,360,600] 1:[360,0,240,600] 2:[600,0,300,600] ",
          "pane 0 grows, pane 1 shrinks, pane 2 untouched",
          Layout(tree, 900, 600));
    Check(Layout(tree, 1800, 600) ==
              "0:[0,0,720,600] 1:[720,0,480,600] 2:[1200,0,600,600] ",
          "the resized proportions hold in a wider window",
          Layout(tree, 1800, 600));
    Node clamped = FlatTree(3, Axis::kHorizontal);
    ResizeSeam(clamped, seam, -1000);
    Check(Layout(clamped, 900, 600).rfind("0:[0,0,64,600]", 0) == 0,
          "a pane is never dragged below the minimum",
          Layout(clamped, 900, 600));
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
  // Random edits: the pane set never changes, the tree stays canonical, and a
  // second Normalize changes nothing.
  {
    // NOLINTNEXTLINE(bugprone-random-generator-seed,cert-msc32-c,cert-msc51-cpp)
    std::mt19937 generator(kRandomSeed);
    for (int run = 0; run < kRandomRuns; run++) {
      Node tree =
          FlatTree(2 + static_cast<int>(generator() % 2), Axis::kHorizontal);
      int nextLeaf = static_cast<int>(tree.children.size());
      for (int step = 0; step < kEditsPerRun; step++) {
        std::vector<int> leaves;
        CollectLeaves(tree, leaves);
        const int moving = leaves[generator() % leaves.size()];
        const int target = leaves[generator() % leaves.size()];
        const auto direction =
            static_cast<Direction>(generator() % kDirectionCount);
        switch (static_cast<Edit>(generator() % kEditCount)) {
          case Edit::kSplit: {
            const bool hasRoomForPane = nextLeaf < kMaxPanes;
            if (hasRoomForPane) {
              SplitPane(tree, moving, direction, nextLeaf);
              nextLeaf++;
            }
            break;
          }
          case Edit::kMove:
            MovePane(tree, moving, target, direction);
            break;
          case Edit::kMoveToRootEdge:
            MovePaneToRootEdge(tree, moving, direction);
            break;
          case Edit::kSwap:
            SwapLeaves(tree, moving, target);
            break;
        }
        std::vector<int> after;
        CollectLeaves(tree, after);
        std::sort(after.begin(), after.end());
        std::vector<int> expected(static_cast<size_t>(nextLeaf));
        for (int i = 0; i < nextLeaf; i++) {
          expected[static_cast<size_t>(i)] = i;
        }
        Check(after == expected, "random: pane set preserved", Dump(tree));
        Check(Canonical(tree), "random: canonical", Dump(tree));
        Node copy = tree;
        Normalize(copy);
        Check(Dump(copy) == Dump(tree), "random: normalize idempotent",
              Dump(tree));
      }
    }
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
