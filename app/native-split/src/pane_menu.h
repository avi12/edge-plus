// pane_menu.h — Edge's own items on a pane's ⋯ menu (SplitWindowControlMenu),
// fitted to the N-pane split. One concern: which of Edge's items the menu
// keeps, and what they say. Its "Add pane" row is menu.h's.
//   - No split offers "Send feedback".
//   - A split of three or more drops the items another control does there:
//     "Separate two tabs" (the toolbar's split button), "Switch left and right
//     tabs" (it only ever swaps the first two panes; dragging a pane onto
//     another swaps any two) and "Switch to vertical / horizontal" (a grown
//     split is laid out by its tree, which ignores Edge's orientation).
//   - The link toggle says what links.h made it do.
//   - "Open this webpage in a new tab" copies the menu's own pane. Edge's
//     copies the first pane from the first pane's ⋯ and the LAST pane from any
//     other's (DoActionToSplitWindow ignores the page it is handed), which past
//     two panes is often another pane's page.
// docs/re/pane-menu.md.
#pragma once
#include <cstddef>
#include "edge.h"
#include "links.h"
#include "pane_tree.h"
#include "split_data.h"

namespace fixedge::pane_menu {

// Edge's items a split of three or more leaves out.
inline constexpr edge155::off::ControlMenuCommand kGrownSplitRepeats[] = {
    edge155::off::ControlMenuCommand::kSeparate,
    edge155::off::ControlMenuCommand::kSwitchSides,
    edge155::off::ControlMenuCommand::kSwitchToVertical,
    edge155::off::ControlMenuCommand::kSwitchToHorizontal,
};

// Whether `mcv` (null when unknown) shows a split of three or more panes.
inline bool IsGrownSplit(void* mcv) {
  return mcv && edge::McvActiveSplitPanes(mcv) > split_data::kMinSplitMembers;
}

// Whether the menu leaves Edge's item `command` out.
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

// The link toggle's label, built once into a static u16string (a short
// permanent allocation), the way menu.h builds its own labels.
inline constinit edge::U16String g_linkModeLabel;
inline constinit bool g_hasLinkModeLabel = false;
inline const edge::U16String* LinkModeLabel() {
  if (!g_hasLinkModeLabel) {
    edge::Utf8ToUtf16(&g_linkModeLabel, links::kToggleLabel);
    g_hasLinkModeLabel = true;
  }
  return &g_linkModeLabel;
}

// Fit the ⋯ menu Edge just built (`model`, before PopulateMenu makes its item
// views) to the split `mcv` shows (null when unknown: a split that never grew).
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

// SplitWindowControlMenu::ExecuteCommand, for the ⋯ menu of `mcv`'s container
// `pane` (mcv null when unknown): runs `command` here if it acts on that pane,
// and returns whether it did; else Edge runs it.
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

}  // namespace fixedge::pane_menu
