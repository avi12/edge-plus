// Unit tests for pane_tree.h.
#include <cstdio>
#include "../../src/layout/pane_edits.h"
#include "../../src/layout/pane_tree.h"

using namespace edgeplus;

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

void RunChecks() {
  // Leaf count: what LiveTree compares against the panes it must tile.
  {
    Check(LeafCount(MakeLeaf(0)) == 1, "a leaf tiles one pane");
    Check(LeafCount(FlatTree(4, Axis::kHorizontal)) == 4,
          "a flat split tiles its count");
    Node tree = FlatTree(2, Axis::kHorizontal);
    SplitPane(tree, 1, Direction::kDown, 2);
    Check(LeafCount(tree) == 3, "a nested split counts every leaf");
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
