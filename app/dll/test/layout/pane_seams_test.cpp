// Unit tests for pane_seams.h.
#include <cstdio>
#include <string>
#include <vector>
#include "../../src/layout/pane_edits.h"
#include "../../src/layout/pane_layout.h"
#include "../../src/layout/pane_seams.h"
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

void RunChecks() {
  // Seams: a tree of N panes has N - 1, each between two neighbours.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    SplitPane(tree, 2, Direction::kDown, 3);  // H[0 1 V[2 3]]
    const std::vector<Seam> seams = Seams(tree, Rect{0, 0, 900, 600}, 0);
    Check(seams.size() == 3, "four panes, three seams");
    Check(seams[0].axis == Axis::kHorizontal && seams[0].position == 300 &&
              seams[0].crossLength == 600,
          "seam between 0 and 1: x=300, full height");
    Check(seams[2].axis == Axis::kVertical && seams[2].position == 300 &&
              seams[2].crossStart == 600 && seams[2].crossLength == 300,
          "seam between 2 and 3: y=300, the third column's width");
  }

  // Laid out kPaneGap apart, a seam is the strip between its two panes: no
  // pane covers it (a page would take the pointer from its divider).
  {
    const Node tree = FlatTree(3, Axis::kHorizontal);
    const Rect area{0, 0, 900 + 2 * kPaneGap, 600};
    const std::vector<Seam> seams = Seams(tree, area, kPaneGap);
    std::vector<LeafRect> leaves;
    LayoutTree(tree, area, kPaneGap, leaves);
    Check(seams[0].position == 300 && seams[0].thickness == kPaneGap,
          "seam 0 starts where pane 0 ends");
    Check(seams[0].position + seams[0].thickness == leaves[1].rect.x,
          "and ends where pane 1 starts");
    Check(seams[1].position == 600 + kPaneGap && seams[1].lengthBefore == 300,
          "seam 1 follows pane 1, both panes 300 wide");
    Node resized = tree;
    Check(ResizeSeam(resized, seams[0], 60), "drag seam 0 by +60");
    Check(Seams(resized, area, kPaneGap)[0].position == 360,
          "the strip moves with the pointer");
  }

  // Dragging a seam trades length between its two panes only.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    const Seam seam = Seams(tree, Rect{0, 0, 900, 600}, 0)[0];
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
    Check(Layout(clamped, 900, 600).starts_with("0:[0,0,64,600]"),
          "a pane is never dragged below the minimum",
          Layout(clamped, 900, 600));
  }

  // Double-clicking a divider evens out exactly the split it divides.
  {
    Node tree = FlatTree(3, Axis::kHorizontal);
    SplitPane(tree, 2, Direction::kDown, 3);  // H[0 1 V[2 3]]
    const Rect area{0, 0, 900, 600};
    std::vector<Seam> seams = Seams(tree, area, 0);
    ResizeSeam(tree, seams[0], 120);   // widen pane 0
    ResizeSeam(tree, seams[2], -100);  // shorten pane 2
    Check(EqualizeSplit(tree, seams[2].path), "reset the column's heights");
    Check(Layout(tree, 900, 600).find("2:[600,0,300,300]") != std::string::npos,
          "the column is even again", Layout(tree, 900, 600));
    Check(Layout(tree, 900, 600).starts_with("0:[0,0,420,600]"),
          "the row's resize is untouched", Layout(tree, 900, 600));
    Check(EqualizeSplit(tree, seams[0].path), "reset the row's widths");
    Check(!EqualizeSplit(tree, seams[0].path), "already even: nothing to do");
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
