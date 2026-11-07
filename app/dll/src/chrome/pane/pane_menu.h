// pane_menu.h — Edge's own items on a pane's ⋯ menu (SplitWindowControlMenu),
// fitted to the N-pane split. Its "Add pane" row is menu.h's.
//   - A split of three or more drops the items another control does there:
//     "Separate two tabs" (the toolbar's split button), "Switch left and right
//     tabs" (it only ever swaps the first two panes; dragging a pane onto
//     another swaps any two) and "Switch to vertical / horizontal" (a grown
//     split is laid out by its tree, which ignores Edge's orientation).
//   - "Open this webpage in a new tab" copies the menu's own pane. Edge's
//     copies the first pane from the first pane's ⋯ and the LAST pane from any
//     other's (DoActionToSplitWindow ignores the page it is handed), which past
//     two panes is often another pane's page.
// docs/re/pane-menu.md.
#pragma once
#include <cstddef>
#include "edge/edge.h"
#include "split/links.h"
#include "layout/pane_tree.h"
#include "split/split_data.h"

namespace edgeplus::pane_menu {

inline constexpr edge155::off::ControlMenuCommand kGrownSplitRepeats[] = {
    edge155::off::ControlMenuCommand::kSeparate,
    edge155::off::ControlMenuCommand::kSwitchSides,
    edge155::off::ControlMenuCommand::kSwitchToVertical,
    edge155::off::ControlMenuCommand::kSwitchToHorizontal,
};

// `mcv` is null when unknown.
inline bool IsGrownSplit(void* mcv) {
  return mcv && edge::McvActiveSplitPanes(mcv) > split_data::kMinSplitMembers;
}

inline bool IsLeftOut(int command, bool isGrownSplit) {
  const bool isFeedback =
      command ==
      static_cast<int>(edge155::off::ControlMenuCommand::kSendFeedback);
  if (isFeedback) {
    return true;
  }
  if (!isGrownSplit) {
    return false;
  }
  for (const edge155::off::ControlMenuCommand repeat : kGrownSplitRepeats) {
    const bool isRepeat = command == static_cast<int>(repeat);
    if (isRepeat) {
      return true;
    }
  }
  return false;
}

// Built once: a short permanent allocation, as menu.h builds its labels.
inline constinit edge::U16String g_linkModeLabel;
inline constinit bool g_hasLinkModeLabel = false;
inline const edge::U16String* LinkModeLabel() {
  if (!g_hasLinkModeLabel) {
    edge::Utf8ToUtf16(&g_linkModeLabel, links::kToggleLabel);
    g_hasLinkModeLabel = true;
  }
  return &g_linkModeLabel;
}

// Before PopulateMenu makes the item views. `mcv` is null for a split that
// never grew.
inline void FitItems(void* model, void* mcv) {
  const bool isGrownSplit = IsGrownSplit(mcv);
  // From the last item, so a removal never shifts an index still to be read.
  for (size_t i = edge::MenuItemCount(model); i > 0; i--) {
    const size_t index = i - 1;
    const int command = edge::MenuCommandIdAt(model, index);
    if (IsLeftOut(command, isGrownSplit)) {
      edge::MenuRemoveItemAt(model, index);
      continue;
    }
    const bool isLinkMode =
        command ==
        static_cast<int>(edge155::off::ControlMenuCommand::kLinkMode);
    if (isLinkMode) {
      edge::MenuSetLabel(model, index, LinkModeLabel());
    }
  }
}

// SplitWindowControlMenu::ExecuteCommand: false leaves `command` to Edge.
inline bool RunItem(int command, void* mcv, int pane) {
  const bool isCopyOfMenuPane =
      command ==
          static_cast<int>(edge155::off::ControlMenuCommand::kOpenInNewTab) &&
      mcv && pane != kNoLeaf;
  if (!isCopyOfMenuPane) {
    return false;
  }
  void* contents = edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
  void* tsm = edge::McvTabStripModel(mcv);
  const bool canCopy = contents && tsm;
  if (!canCopy) {
    return false;
  }
  // A background tab at the end of the strip, where Edge's own copy lands.
  edge::AddBackgroundTabForGurl(edge::BrowserOf(edge::McvBrowserView(mcv)),
                                edge::LastCommittedUrl(contents),
                                edge::TabCount(tsm));
  Log("[pane menu] pane %d's page copied into a new tab", pane);
  return true;
}

}  // namespace edgeplus::pane_menu
