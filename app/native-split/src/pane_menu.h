// pane_menu.h — Edge's own items on a pane's ⋯ menu (SplitWindowControlMenu),
// fitted to the N-pane split. One concern: which of Edge's items the menu
// keeps. Its "Add pane" row is menu.h's.
//   - No split offers "Send feedback".
//   - A split of three or more drops the items another control does there:
//     "Separate two tabs" (the toolbar's split button), "Switch left and right
//     tabs" (it only ever swaps the first two panes; dragging a pane onto
//     another swaps any two) and "Switch to vertical / horizontal" (a grown
//     split is laid out by its tree, which ignores Edge's orientation).
// docs/re/pane-menu.md.
#pragma once
#include <cstddef>
#include "edge.h"
#include "split_data.h"

namespace fixedge::pane_menu {

using namespace fixedge::edge155;
using off::ControlMenuCommand;

// Edge's items a split of three or more leaves out.
inline constexpr ControlMenuCommand kGrownSplitRepeats[] = {
    ControlMenuCommand::kSeparate,
    ControlMenuCommand::kSwitchSides,
    ControlMenuCommand::kSwitchToVertical,
    ControlMenuCommand::kSwitchToHorizontal,
};

// Whether `mcv` (null when unknown) shows a split of three or more panes.
inline bool IsGrownSplit(void* mcv) {
  return mcv && edge::McvActiveSplitPanes(mcv) > split_data::kMinSplitMembers;
}

// Whether the menu leaves Edge's item `command` out.
inline bool IsLeftOut(int command, bool isGrownSplit) {
  const bool isFeedback =
      command == static_cast<int>(ControlMenuCommand::kSendFeedback);
  if (isFeedback) {
    return true;
  }
  if (!isGrownSplit) {
    return false;
  }
  for (const ControlMenuCommand repeat : kGrownSplitRepeats) {
    const bool isRepeat = command == static_cast<int>(repeat);
    if (isRepeat) {
      return true;
    }
  }
  return false;
}

// Fit the ⋯ menu Edge just built (`model`, before PopulateMenu makes its item
// views) to the split `mcv` shows (null when unknown: a split that never grew).
inline void FitItems(void* model, void* mcv) {
  const bool isGrownSplit = IsGrownSplit(mcv);
  // From the last item, so a removal never shifts an index still to be read.
  for (size_t i = edge::MenuItemCount(model); i > 0; i--) {
    const size_t index = i - 1;
    const bool isLeftOut =
        IsLeftOut(edge::MenuCommandIdAt(model, index), isGrownSplit);
    if (isLeftOut) {
      edge::MenuRemoveItemAt(model, index);
    }
  }
}

}  // namespace fixedge::pane_menu
