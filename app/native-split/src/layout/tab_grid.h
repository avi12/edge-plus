// tab_grid.h — the grid a split's tabs take in the tab list: how many tabs sit
// on each row, and which pane's tab takes each place. Pure (no Edge, no Win32).
// A split with a pane layout tree of its own mirrors its panes' rows and columns
// (3 panes side by side are one row of 3, a 2x2 split two rows of 2); a flat
// split is one row of its panes when each tab still gets a usable width, else it
// wraps into as many rows as that width allows.
#pragma once
#include <algorithm>
#include <vector>
#include "layout/pane_tree.h"

namespace edgeplus::tab_grid {

// Room for the icon, a few letters and the close button.
inline constexpr int kMinSlotWidth = 100;

struct TabGrid {
  // Which member each place takes, in reading order of the places (rows top to
  // bottom, places left to right); empty for strip order.
  std::vector<int> order;
  // How many tabs each row holds, top row first.
  std::vector<int> rowSizes;
};

// `count` tabs over rows of at most `perRow`, shared out evenly with the earlier
// rows taking the remainder.
[[nodiscard]] inline std::vector<int> WrappedRowSizes(int count, int perRow) {
  const int rows = (count + perRow - 1) / perRow;
  std::vector<int> rowSizes;
  int placed = 0;
  for (int row = 0; row < rows; row++) {
    const int remaining = count - placed;
    const int size = (remaining + (rows - row) - 1) / (rows - row);
    rowSizes.push_back(size);
    placed += size;
  }
  return rowSizes;
}

// A split with no tree of its own: its panes in strip order, on one row if
// `rowWidth` gives each tab `kMinSlotWidth`, else wrapped.
[[nodiscard]] inline TabGrid GridOfFlatSplit(int count, int rowWidth) {
  const int fitting = std::max(1, rowWidth / kMinSlotWidth);
  return TabGrid{{}, WrappedRowSizes(count, std::min(count, fitting))};
}

// The panes of `tree` as they sit on screen: panes sharing a top edge are a row.
[[nodiscard]] inline TabGrid GridOfTree(const Node& tree) {
  std::vector<LeafRect> leaves;
  LayoutTree(tree, Rect{0, 0, kReadingOrderExtent, kReadingOrderExtent}, 0,
             leaves);
  std::stable_sort(leaves.begin(), leaves.end(),
                   [](const LeafRect& first, const LeafRect& second) {
                     if (first.rect.y != second.rect.y) {
                       return first.rect.y < second.rect.y;
                     }
                     return first.rect.x < second.rect.x;
                   });
  TabGrid grid;
  for (size_t i = 0; i < leaves.size(); i++) {
    const bool isNewRow = i == 0 || leaves[i].rect.y != leaves[i - 1].rect.y;
    if (isNewRow) {
      grid.rowSizes.push_back(0);
    }
    grid.rowSizes.back()++;
    grid.order.push_back(leaves[i].leaf);
  }
  return grid;
}

}  // namespace edgeplus::tab_grid
