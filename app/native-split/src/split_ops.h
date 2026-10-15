// split_ops.h — the compound "add a pane in a direction" action. One concern:
// orchestrate the tree edit + container build + content feed + relayout that a
// directional-add menu command performs. UI thread only.
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

namespace fixedge::split_ops {

using namespace fixedge::edge155;

// The pane (container index) showing `webContents`, or kNoLeaf.
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

// Index of the active pane, the one showing the strip's active tab, else 0.
// The panes' primary flags can lag the strip: Edge may activate a pane while a
// rebuild has it hidden, and hooks.h ignores that, so a hidden pane keeps the
// flag.
inline int ActiveLeaf(void* mcv) {
  void* tsm = edge::McvTabStripModel(mcv);
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const int leaf =
      active ? LeafShowing(mcv, edge::TabContents(active)) : kNoLeaf;
  return leaf != kNoLeaf ? leaf : 0;
}

// Give a newly built pane content from a background tab (from the strip tail, so
// never the active/split tabs). Blank if none spare.
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

// Whether the active tab of `tsm` is in a split.
inline bool IsActiveTabSplit(void* tsm) {
  void* active = edge::ActiveTab(tsm);
  return active && edge::TabIsSplit(active);
}

// Grow the active split by one member, `newTab` (a TabModel*; null: a free
// background tab), and lay the panes out as `tree`, in which the new pane is
// the newest leaf (GrowSplit appends it as the last container). The tree goes
// in first, so the relayout the grow triggers already uses it; a failed grow
// puts back the tree that was there. UI thread.
inline bool GrowWithTree(void* mcv, void* tsm, Node tree, void* newTab) {
  // A copy, not a pointer: the speculative edit below overwrites this entry.
  const Node* existing = IsActiveTabSplit(tsm) ? TreeFor(mcv) : nullptr;
  const std::optional<Node> previousTree =
      existing ? std::optional<Node>(*existing) : std::nullopt;
  Trees()[mcv] = std::move(tree);
  // The feeder builds the new container, feeds it real content and shows it;
  // our layout detour positions it by the tree above.
  const bool hasGrown = split_data::GrowSplit(tsm, newTab);
  if (!hasGrown) {
    if (previousTree) {
      Trees()[mcv] = *previousTree;
    } else {
      ForgetTree(mcv);  // undo the speculative edit
    }
    Log("[grow] failed; tree reverted");
    return false;
  }
  edge::InvalidateLayout(mcv);
  // A pane's tab is no longer one an open tab offer may list.
  if (newTab) {
    edge::WithdrawFromStartingPages(
        edge::SplitWindowControllerFor(
            edge::BrowserOf(edge::McvBrowserView(mcv))),
        edge::TabContents(newTab));
  }
  return true;
}

// Add one pane beside pane `besideLeaf`, in `direction`, as a real split-tree
// edit: `newTab` (null: a free background tab) becomes the new pane. Unsplit,
// the one pane (the active tab) is container 0. Whether the pane was added.
// UI thread.
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
  const bool hasGrown = GrowWithTree(mcv, tsm, std::move(tree), newTab);
  if (hasGrown) {
    Log("[dir-add] dir=%d active_leaf=%d new_leaf=%d -> %d panes",
        static_cast<int>(direction), activeLeaf, newLeaf, oldPanes + 1);
  }
  return hasGrown;
}

// Add one pane beside the active pane.
inline void AddPane(void* mcv, Direction direction) {
  AddPane(mcv, direction, mcv ? ActiveLeaf(mcv) : 0);
}

// Move focus to the pane on `direction`'s side of the active one — the native
// equivalent of the extension's Alt+Shift+arrow pane focus. The target is
// picked from the panes' on-screen rects (DirectionalNeighbour), then its
// member tab is activated, which focuses that pane the way a click does.
// No-op when the active tab is not in a split or there is no pane that way.
// Returns whether focus moved. UI thread.
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

// Swap the active pane with its sibling along its parent split's axis (the
// position-aware "counterpart"). Pure tree edit: the leaves' container indices
// swap position, so the two panes' content trades places on relayout. No Edge
// object moves. No-op without a live tree. UI thread.
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
    Trees()[mcv] = tree;
    edge::InvalidateLayout(mcv);
    edge::LayoutImmediately(mcv);  // no data-model change, so force the re-tile
    Log("[swap] swapped pane %d with its counterpart", activeLeaf);
  } else {
    Log("[swap] pane %d has no counterpart", activeLeaf);
  }
}

// The pane that takes over activity when `closing` closes: the one before it in
// reading order, else the one after.
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

// Close pane `pane` of a split of three or more: only that pane's tab closes,
// its siblings share its space in proportion, and the rest of the layout stays.
// Containers after the closed one are re-fed one slot earlier; the transition
// eases every pane from where its tab was drawn to its new place. Returns false to leave a two-pane split to Edge. UI thread.
inline bool ClosePane(void* mcv, int pane) {
  void* tsm = edge::McvTabStripModel(mcv);
  const int memberCount = mcv ? edge::McvActiveSplitPanes(mcv) : 0;
  const bool isGrownSplit = tsm && memberCount > 2 && pane < memberCount;
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
      split_data::CloseMember(tsm, closingContents, activeContents);
  if (remaining == 0) {
    return false;
  }
  RemovePane(tree, pane);
  Trees()[mcv] = tree;
  anim::ForgetTab(closingContents);
  // Laid out on Edge's own schedule, after it re-feeds the containers; each
  // pane then eases from where its tab was (anim::LastDrawn).
  edge::InvalidateLayout(mcv);
  Log("[close] pane %d closed; %d panes remain", pane, remaining);
  return true;
}

}  // namespace fixedge::split_ops
