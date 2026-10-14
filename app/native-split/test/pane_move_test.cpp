// Unit tests for the drag-and-drop tree edits in pane_tree.h (MovePane,
// MovePaneToRootEdge, SwapLeaves, Normalize). No Edge/Win32 deps. Run via
// test\run.cmd.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include "../src/pane_tree.h"

using namespace fixedge;

namespace {

int g_checks = 0;
void Check(bool ok, const char* what, const std::string& got = "") {
  ++g_checks;
  if (!ok) {
    std::printf("FAIL: %s %s\n", what, got.c_str());
    std::exit(1);
  }
}

// H[0(1) 1(1) V[2(1) 3(1)](1)] — axis, leaves and slot weights.
std::string Dump(const Node& n) {
  if (n.is_leaf) return std::to_string(n.leaf);
  std::string s = n.axis == Axis::kHorizontal ? "H[" : "V[";
  for (size_t i = 0; i < n.children.size(); ++i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "(%.3g)", n.weights[i]);
    if (i) s += " ";
    s += Dump(n.children[i]) + buf;
  }
  return s + "]";
}

std::string Layout(const Node& root, int w, int h) {
  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, w, h}, 0, leaves);
  std::string s;
  for (const LeafRect& lr : leaves) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d:[%d,%d,%d,%d] ", lr.leaf, lr.rect.x, lr.rect.y, lr.rect.w,
                  lr.rect.h);
    s += buf;
  }
  return s;
}

// The canonical form every edit must leave behind.
bool Canonical(const Node& n) {
  if (n.is_leaf) return true;
  if (n.children.size() < 2 || n.weights.size() != n.children.size()) return false;
  float sum = 0.f;
  for (float w : n.weights) {
    if (!(w > 0.f)) return false;
    sum += w;
  }
  if (std::fabs(sum - static_cast<float>(n.children.size())) > 1e-3f) return false;
  for (const Node& c : n.children) {
    if (!c.is_leaf && c.axis == n.axis) return false;
    if (!Canonical(c)) return false;
  }
  return true;
}

Node Weighted(int count, Axis axis, std::vector<float> weights) {
  Node t = FlatTree(count, axis);
  t.weights = std::move(weights);
  FixWeights(t);
  return t;
}

}  // namespace

int main() {
  // The user's scenario (a): row 1..4, drop 4 on the bottom edge of 3 -> thirds
  // across, and 3 / 4 share the third column at equal heights.
  {
    Node t = FlatTree(4, Axis::kHorizontal);
    Check(MovePane(t, 3, 2, Direction::kDown), "drop below 3 moved");
    Check(Dump(t) == "H[0(1) 1(1) V[2(1) 3(1)](1)]", "drop below 3 tree", Dump(t));
    Check(Layout(t, 1200, 800) ==
              "0:[0,0,400,800] 1:[400,0,400,800] 2:[800,0,400,400] 3:[800,400,400,400] ",
          "drop below 3 geometry", Layout(t, 1200, 800));
  }
  // The user's scenario (b): drop 4 on the bottom edge of the whole layout -> it
  // takes the full bottom half; 1..3 share the top half in thirds.
  {
    Node t = FlatTree(4, Axis::kHorizontal);
    Check(MovePaneToRootEdge(t, 3, Direction::kDown), "root bottom moved");
    Check(Dump(t) == "V[H[0(1) 1(1) 2(1)](1) 3(1)]", "root bottom tree", Dump(t));
    Check(Layout(t, 1200, 800) ==
              "0:[0,0,400,400] 1:[400,0,400,400] 2:[800,0,400,400] 3:[0,400,1200,400] ",
          "root bottom geometry", Layout(t, 1200, 800));
  }
  // No-ops leave the tree untouched.
  {
    Node t = FlatTree(4, Axis::kHorizontal);
    const std::string before = Dump(t);
    Check(!MovePane(t, 2, 2, Direction::kDown), "drop on itself");
    Check(!MovePane(t, 3, 2, Direction::kRight), "already right of target");
    Check(!MovePane(t, 2, 3, Direction::kLeft), "already left of target");
    Check(!MovePaneToRootEdge(t, 3, Direction::kRight), "already spans right edge");
    Check(!MovePaneToRootEdge(t, 0, Direction::kLeft), "already spans left edge");
    Check(!MovePane(t, 9, 2, Direction::kDown), "missing moving pane");
    Check(!MovePane(t, 2, 9, Direction::kDown), "missing target pane");
    Check(!SwapLeaves(t, 1, 1), "swap with itself");
    Check(!SwapLeaves(t, 1, 9), "swap with missing");
    Check(Dump(t) == before, "no-ops leave the tree unchanged", Dump(t));
    Node one = MakeLeaf(0);
    Check(!MovePaneToRootEdge(one, 0, Direction::kDown), "single pane has no root edge");
    Node v = Weighted(2, Axis::kVertical, {3.f, 1.f});
    Check(!MovePane(v, 1, 0, Direction::kDown), "already below -> keeps 3:1");
  }
  // A reorder within one split carries the pane's own size.
  {
    Node t = Weighted(3, Axis::kHorizontal, {3.f, 1.f, 1.f});
    Check(MovePane(t, 0, 2, Direction::kRight), "reorder moved");
    Check(Dump(t) == "H[1(0.6) 2(0.6) 0(1.8)]", "reorder keeps weight", Dump(t));
  }
  // A 2-pane column turned into a row (the emptied split collapses).
  {
    Node t = FlatTree(2, Axis::kVertical);
    Check(MovePane(t, 1, 0, Direction::kRight), "pair transpose moved");
    Check(Dump(t) == "H[0(1) 1(1)]", "V[0,1] -> H[0,1]", Dump(t));
  }
  // Removing a pane flattens what is left without changing on-screen widths.
  {
    Node t = FlatTree(2, Axis::kHorizontal);
    SplitPane(t, 1, Direction::kDown, 2);   // H[0, V[1,2]]
    SplitPane(t, 2, Direction::kRight, 3);  // H[0, V[1, H[2,3]]]
    Check(Canonical(t), "setup canonical", Dump(t));
    Check(MovePaneToRootEdge(t, 1, Direction::kDown), "flatten move");
    Check(Dump(t) == "V[H[0(1.5) 2(0.75) 3(0.75)](1) 1(1)]", "flatten keeps widths", Dump(t));
    Check(Layout(t, 800, 400) ==
              "0:[0,0,400,200] 2:[400,0,200,200] 3:[600,0,200,200] 1:[0,200,800,200] ",
          "flatten geometry", Layout(t, 800, 400));
  }
  // Removal shares the freed space in proportion (a 2:1 pair stays 2:1).
  {
    Node t = Weighted(3, Axis::kHorizontal, {2.f, 1.f, 1.f});
    MovePaneToRootEdge(t, 2, Direction::kDown);
    Check(Dump(t) == "V[H[0(1.33) 1(0.667)](1) 2(1)]", "proportional removal", Dump(t));
  }
  // Joining a resized row only halves the target; an equal row stays equal.
  {
    Node t = Weighted(2, Axis::kHorizontal, {3.f, 1.f});
    SplitPane(t, 1, Direction::kDown, 2);
    MovePane(t, 2, 1, Direction::kRight);
    Check(Dump(t) == "H[0(2.25) 1(0.375) 2(0.375)]", "resized row: half of target", Dump(t));
    Node e = FlatTree(3, Axis::kHorizontal);
    SplitPane(e, 2, Direction::kDown, 3);
    MovePane(e, 3, 1, Direction::kRight);
    Check(Dump(e) == "H[0(1) 1(1) 3(1) 2(1)]", "equal row: quarters", Dump(e));
  }
  // A root along the drop axis gains the pane as an average-share end child.
  {
    Node t = FlatTree(2, Axis::kHorizontal);
    MovePaneToRootEdge(t, 1, Direction::kDown);  // V[0,1]
    SplitPane(t, 0, Direction::kRight, 2);       // V[H[0,2],1]
    MovePaneToRootEdge(t, 0, Direction::kDown);  // V[2,1,0]
    Check(Dump(t) == "V[2(1) 1(1) 0(1)]", "root join -> thirds", Dump(t));
  }
  // Centre drop: positions keep their sizes, the panes trade places.
  {
    Node t = Weighted(3, Axis::kHorizontal, {2.f, 1.f, 1.f});
    Check(SwapLeaves(t, 0, 2), "swap moved");
    Check(Dump(t) == "H[2(1.5) 1(0.75) 0(0.75)]", "swap keeps sizes with positions", Dump(t));
  }
  // Random edits: the pane set never changes, the tree stays canonical, and a
  // second Normalize changes nothing.
  {
    std::mt19937 rng(12345);
    for (int iter = 0; iter < 200; ++iter) {
      Node t = FlatTree(2 + static_cast<int>(rng() % 2), Axis::kHorizontal);
      int next = static_cast<int>(t.children.size());
      for (int step = 0; step < 60; ++step) {
        std::vector<int> leaves;
        CollectLeaves(t, leaves);
        const int a = leaves[rng() % leaves.size()];
        const int b = leaves[rng() % leaves.size()];
        const Direction d = static_cast<Direction>(rng() % 4);
        switch (rng() % 4) {
          case 0:
            if (next < 9) SplitPane(t, a, d, next++);
            break;
          case 1: MovePane(t, a, b, d); break;
          case 2: MovePaneToRootEdge(t, a, d); break;
          default: SwapLeaves(t, a, b); break;
        }
        std::vector<int> after;
        CollectLeaves(t, after);
        std::sort(after.begin(), after.end());
        std::vector<int> want(static_cast<size_t>(next));
        for (int i = 0; i < next; ++i) want[static_cast<size_t>(i)] = i;
        Check(after == want, "random: pane set preserved", Dump(t));
        Check(Canonical(t), "random: canonical", Dump(t));
        Node copy = t;
        Normalize(copy);
        Check(Dump(copy) == Dump(t), "random: normalize idempotent", Dump(t));
      }
    }
  }

  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
