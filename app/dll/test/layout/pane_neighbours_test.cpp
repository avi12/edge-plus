// Unit tests for pane_neighbours.h.
#include <cstdio>
#include <string>
#include <vector>
#include "../../src/layout/pane_edits.h"
#include "../../src/layout/pane_layout.h"
#include "../../src/layout/pane_neighbours.h"
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

void RunChecks() {
  // Directional neighbour (pane focus): the pane on each side, by on-screen
  // rects. Tree H[0 1 V[2 3]] lays out as two tall columns (0, 1) and a stacked
  // pair (2 over 3) in the third column.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    SplitPane(tree, 2, Direction::kDown, 3);  // H[0 1 V[2 3]]
    std::vector<LeafRect> leaves;
    LayoutTree(tree, Rect{0, 0, 900, 600}, 0, leaves);
    const auto neighbour = [&](int from, Direction direction) {
      return DirectionalNeighbour(leaves, from, direction);
    };
    Check(neighbour(0, Direction::kRight) == 1, "right of 0 is 1");
    Check(neighbour(1, Direction::kLeft) == 0, "left of 1 is 0");
    Check(neighbour(1, Direction::kRight) == 2, "right of 1 is the top stack");
    Check(neighbour(2, Direction::kDown) == 3, "down of 2 is 3");
    Check(neighbour(3, Direction::kUp) == 2, "up of 3 is 2");
    Check(neighbour(2, Direction::kLeft) == 1, "left of a stacked pane is 1");
    Check(neighbour(3, Direction::kLeft) == 1, "left of the lower stack is 1");
    Check(neighbour(0, Direction::kUp) == kNoLeaf, "nothing above a column");
    Check(neighbour(0, Direction::kLeft) == kNoLeaf, "nothing left of pane 0");
    Check(neighbour(2, Direction::kRight) == kNoLeaf,
          "nothing right of pane 2");
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
