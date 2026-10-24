// split_ops.h — compound pane actions: the tree edit, data-model change, content
// feed and relayout that adding, moving or closing a pane takes. UI thread only.
#pragma once
#include <optional>
#include "edge.h"
#include "panes.h"
#include "tabs.h"
#include "state.h"
#include "pane_tree.h"
#include "geometry.h"
#include "split_data.h"
#include "anim.h"
#include "repaint.h"

namespace edgeplus::split_ops {

inline int LeafShowing(void* mcv, const void* webContents) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  for (int i = 0; i < count; i++) {
    const bool isShowing =
        edge::ContainerWebContents(edge::McvContainerAt(mcv, i)) == webContents;
    if (isShowing) {
      return i;
    }
  }
  return kNoLeaf;
}

// The pane showing the strip's active tab, else 0. Not the panes' primary flag,
// which can lag the strip: Edge may activate a pane while a rebuild has it
// hidden, and hooks.h ignores that.
inline int ActiveLeaf(void* mcv) {
  void* tsm = edge::McvTabStripModel(mcv);
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const int leaf =
      active ? LeafShowing(mcv, edge::TabContents(active)) : kNoLeaf;
  return leaf != kNoLeaf ? leaf : 0;
}

// From the strip's tail, so never the active or split tabs; blank if none spare.
inline void FeedNewPane(void* mcv, int leaf) {
  void* browserView = edge::McvBrowserView(mcv);
  void* tsm = edge::TabStripModelOf(edge::BrowserOf(browserView));
  const int tabCount = tsm ? edge::TabCount(tsm) : 0;
  Log("[feed] FeedNewPane leaf=%d bv=%p tsm=%p tabs=%d", leaf, browserView, tsm,
      tabCount);
  for (int tabIndex = tabCount - 1; tabIndex >= 0; tabIndex--) {
    const bool isFilled =
        tabs::FillPane(browserView, edge::McvContainerAt(mcv, leaf), tabIndex);
    if (isFilled) {
      Log("[feed] fed leaf %d from tab %d", leaf, tabIndex);
      return;
    }
  }
  Log("[feed] no background tab fed into leaf %d", leaf);
}

inline bool IsActiveTabSplit(void* tsm) {
  void* active = edge::ActiveTab(tsm);
  return active && edge::TabIsSplit(active);
}

// Every way a pane is added comes through here.
inline void OnPaneAdded(void* mcv, void* newTab) {
  edge::InvalidateLayout(mcv);
  // The new pane's page was shown when it was fed, but Edge hides it again and
  // its growth resizes it every frame: it stays blank until it is nudged once
  // the layout has settled.
  repaint::ScheduleAll();
  // An open tab offer must no longer list a pane's tab.
  if (newTab) {
    edge::WithdrawFromStartingPages(
        edge::SplitWindowControllerFor(
            edge::BrowserOf(edge::McvBrowserView(mcv))),
        edge::TabContents(newTab));
  }
}

// A plain tab and `newTab` as a new split, as the two-leaf `tree` places them
// (the tab leaf 0, `newTab` leaf 1). Edge lays out two panes itself, from the
// split's layout and its member order, so the tree it keeps is in that order.
inline bool CreatePair(void* mcv, void* tsm, const Node& tree, void* newTab) {
  Pair pair;
  const bool isPair = ReadPair(tree, pair);
  if (!isPair) {
    return false;
  }
  const split_data::SplitLayout layout = pair.axis == Axis::kHorizontal
                                             ? split_data::SplitLayout::kColumns
                                             : split_data::SplitLayout::kRows;
  HandOverTree(mcv, FlatTree(split_data::kMinSplitMembers, pair.axis));
  const bool isCreated =
      split_data::CreateSplitWith(tsm, newTab, layout, pair.isSecondLeafFirst);
  EndTreeHandOver(mcv);
  if (!isCreated) {
    ForgetTree(mcv);
  }
  return isCreated;
}

// `newTab` (a TabModel*; null: a free background tab) is `tree`'s newest leaf,
// since GrowSplit appends it as the last container. The tree goes in first, so
// the relayout the grow triggers already uses it.
inline bool GrowWithTree(void* mcv, void* tsm, const Node& tree, void* newTab) {
  const bool isNewSplit = !IsActiveTabSplit(tsm);
  if (isNewSplit) {
    const bool isCreated = CreatePair(mcv, tsm, tree, newTab);
    if (isCreated) {
      OnPaneAdded(mcv, newTab);
    }
    return isCreated;
  }
  // A copy, not a pointer: the speculative edit below overwrites this entry.
  const Node* existing = TreeFor(mcv);
  const std::optional<Node> previousTree =
      existing ? std::optional<Node>(*existing) : std::nullopt;
  HandOverTree(mcv, tree);
  const bool hasGrown = split_data::GrowSplit(tsm, newTab);
  EndTreeHandOver(mcv);
  if (!hasGrown) {
    if (previousTree) {
      SetTree(mcv, *previousTree);
    } else {
      ForgetTree(mcv);
    }
    Log("[grow] failed; tree reverted");
    return false;
  }
  OnPaneAdded(mcv, newTab);
  return true;
}

// `newTab` null: a free background tab. Unsplit, the one pane (the active tab)
// is container 0.
inline bool AddPane(void* mcv, Direction direction, int besideLeaf,
                    void* newTab = nullptr) {
  void* tsm = edge::McvTabStripModel(mcv);
  if (!tsm) {
    return false;
  }
  const bool isAlreadySplit = IsActiveTabSplit(tsm);
  const int oldPanes = isAlreadySplit ? edge::McvActiveSplitPanes(mcv) : 1;
  const int activeLeaf = isAlreadySplit ? besideLeaf : 0;
  const int newLeaf = oldPanes;
  Node tree = isAlreadySplit ? geometry::LiveTree(mcv, oldPanes) : MakeLeaf(0);
  const bool hasSplitActiveLeaf =
      SplitPane(tree, activeLeaf, direction, newLeaf);
  if (!hasSplitActiveLeaf) {
    SplitPane(tree, 0, direction, newLeaf);
  }
  const bool hasGrown = GrowWithTree(mcv, tsm, tree, newTab);
  if (hasGrown) {
    Log("[dir-add] dir=%d active_leaf=%d new_leaf=%d -> %d panes",
        static_cast<int>(direction), activeLeaf, newLeaf, oldPanes + 1);
  }
  return hasGrown;
}

inline void AddPane(void* mcv, Direction direction) {
  AddPane(mcv, direction, mcv ? ActiveLeaf(mcv) : 0);
}

// Activating the target's member tab focuses that pane the way a click does.
inline bool FocusNeighbour(void* mcv, Direction direction) {
  void* tsm = mcv ? edge::McvTabStripModel(mcv) : nullptr;
  const int panes = mcv ? edge::McvActiveSplitPanes(mcv) : 0;
  const bool isSplit = tsm && panes >= split_data::kMinSplitMembers;
  if (!isSplit) {
    return false;
  }
  std::vector<LeafRect> leaves;
  LayoutTree(geometry::LiveTree(mcv, panes), geometry::SplitArea(mcv), kPaneGap,
             leaves);
  const int target = DirectionalNeighbour(leaves, ActiveLeaf(mcv), direction);
  if (target == kNoLeaf) {
    return false;
  }
  void* contents =
      edge::ContainerWebContents(edge::McvContainerAt(mcv, target));
  const int stripIndex =
      contents ? edge::IndexOfWebContents(tsm, contents) : -1;
  if (stripIndex < 0) {
    return false;
  }
  edge::ActivateTabAt(tsm, stripIndex);
  Log("[focus] active pane -> %d (direction %d)", target,
      static_cast<int>(direction));
  return true;
}

// After a pure tree edit: the tab list orders a split's tabs by where their
// panes are (tabrows::PaneOrder), and nothing else would lay it out again.
inline void RelayoutTabList(void* mcv) {
  void* tabStrip = edge::BrowserViewTabStrip(edge::McvBrowserView(mcv));
  if (tabStrip) {
    edge::InvalidateLayout(tabStrip);
  }
}

// A pure tree edit: the two leaves trade places on relayout; no Edge object
// moves.
inline void SwapActive(void* mcv) {
  if (!mcv) {
    return;
  }
  const Node* existing = TreeFor(mcv);
  if (!existing) {
    Log("[swap] no live tree");
    return;
  }
  const int activeLeaf = ActiveLeaf(mcv);
  Node tree = *existing;
  const bool hasSwapped = SwapWithCounterpart(tree, activeLeaf);
  if (hasSwapped) {
    SetTree(mcv, tree);
    edge::InvalidateLayout(mcv);
    // No data-model change to trigger the re-tile.
    edge::LayoutImmediately(mcv);
    RelayoutTabList(mcv);
    Log("[swap] swapped pane %d with its counterpart", activeLeaf);
  } else {
    Log("[swap] pane %d has no counterpart", activeLeaf);
  }
}

// Put member i's page back in pane i of a grown split. When CloseMember
// re-creates the split, Edge re-feeds only the member it activates, bypassing
// SetWebContentsAtIndex, and into pane 0. Mismatched panes are emptied first, so
// no page is pulled out of a pane that still needs it. Runs after Edge's own
// re-feed has settled.
inline void SyncPanesToMembers(void* mcv) {
  void* tsm = mcv ? edge::McvTabStripModel(mcv) : nullptr;
  const bool isGrownView =
      tsm && edge::McvCount(mcv) > split_data::kMinSplitMembers;
  if (!isGrownView) {
    return;
  }
  void* contents[split_data::kMemberBufferSize] = {};
  const int memberCount = split_data::ActiveSplitContents(tsm, contents);
  const int paneCount =
      std::min(memberCount, static_cast<int>(edge::McvCount(mcv)));
  bool isMismatched[split_data::kMemberBufferSize] = {};
  int mismatches = 0;
  for (int i = 0; i < paneCount; i++) {
    void* container = edge::McvContainerAt(mcv, i);
    isMismatched[i] = edge::ContainerWebContents(container) != contents[i];
    if (isMismatched[i]) {
      edge::ContentsWebViewSetContents(edge::ContainerWebView(container),
                                       nullptr);
      mismatches++;
    }
  }
  for (int i = 0; i < paneCount; i++) {
    if (!isMismatched[i]) {
      continue;
    }
    edge::ContentsWebViewSetContents(
        edge::ContainerWebView(edge::McvContainerAt(mcv, i)), contents[i]);
    edge::WebContentsWasShown(contents[i]);
  }
  if (mismatches > 0) {
    edge::InvalidateLayout(mcv);
    Log("[sync] re-fed %d of %d panes to match the split", mismatches,
        paneCount);
  }
}

// The one before `closing` in reading order, else the one after.
inline int NeighbourLeaf(const Node& tree, int closing) {
  std::vector<int> order;
  CollectLeaves(tree, order);
  for (size_t i = 0; i < order.size(); i++) {
    if (order[i] != closing) {
      continue;
    }
    if (i > 0) {
      return order[i - 1];
    }
    return i + 1 < order.size() ? order[i + 1] : closing;
  }
  return closing;
}

// Its siblings share its space and the rest of the layout stays. Returns false
// to leave a two-pane split to Edge.
inline bool RemovePaneFromSplit(void* mcv, int pane,
                                split_data::MemberFate fate) {
  void* tsm = edge::McvTabStripModel(mcv);
  const int memberCount = mcv ? edge::McvActiveSplitPanes(mcv) : 0;
  const bool isGrownSplit =
      tsm && memberCount > split_data::kMinSplitMembers && pane < memberCount;
  if (!isGrownSplit) {
    return false;
  }
  Node tree = geometry::LiveTree(mcv, memberCount);
  const int activeLeaf = ActiveLeaf(mcv);
  const int nextActiveLeaf =
      activeLeaf != pane ? activeLeaf : NeighbourLeaf(tree, pane);
  const void* closingContents =
      edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
  const void* activeContents =
      edge::ContainerWebContents(edge::McvContainerAt(mcv, nextActiveLeaf));
  const int remaining =
      split_data::CloseMember(tsm, closingContents, activeContents, fate);
  if (remaining == 0) {
    return false;
  }
  RemovePane(tree, pane);
  SetTree(mcv, tree);
  anim::ForgetTab(closingContents);
  // Laid out on Edge's own schedule, after it re-feeds the containers.
  edge::InvalidateLayout(mcv);
  repaint::ScheduleAll();
  Log("[close] pane %d left the split (%s); %d panes remain", pane,
      fate == split_data::MemberFate::kClose ? "closed" : "kept as a tab",
      remaining);
  return true;
}

inline bool ClosePane(void* mcv, int pane) {
  return RemovePaneFromSplit(mcv, pane, split_data::MemberFate::kClose);
}

inline bool DetachPane(void* mcv, int pane) {
  return RemovePaneFromSplit(mcv, pane, split_data::MemberFate::kKeepAsTab);
}

}  // namespace edgeplus::split_ops
