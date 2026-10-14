// split_ops.h — the compound "add a pane in a direction" action. One concern:
// orchestrate the tree edit + container build + content feed + relayout that a
// directional-add menu command performs. UI thread only.
#pragma once
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

// Index of the active (primary) pane, else 0. The primary flag lives on each
// container's ContentsWebView (set when Edge makes that pane active).
inline int ActiveLeaf(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  for (int i = 0; i < count; i++) {
    void* container = edge::McvContainerAt(mcv, i);
    const bool isPrimaryPane =
        container && edge::Field<uint8_t>(edge::ContainerWebView(container),
                                          off::CWV_primary_flag) == 1;
    if (isPrimaryPane) {
      return i;
    }
  }
  return 0;
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

// Add one pane adjacent to the active pane, in `direction`, as a real split-tree
// edit. The data model grows by one member (GrowSplit, which appends it as the
// last container), and the tree records where that container sits relative to
// the active pane. The layout detour then tiles containers per the tree.
// UI thread.
inline void AddPane(void* mcv, Direction direction) {
  if (!mcv) {
    return;
  }
  void* browserView = edge::McvBrowserView(mcv);
  void* tsm = browserView ? edge::TabStripModelOf(edge::BrowserOf(browserView))
                          : nullptr;
  if (!tsm) {
    return;
  }
  void* active = edge::ActiveTab(tsm);
  const bool isAlreadySplit = active && edge::TabIsSplit(active);
  const int oldPanes = isAlreadySplit ? edge::McvActiveSplitPanes(mcv) : 1;
  const int activeLeaf = isAlreadySplit ? ActiveLeaf(mcv) : 0;
  // GrowSplit appends the new tab as the last container.
  const int newLeaf = oldPanes;

  // Reshape the tree first so the relayout the grow triggers already uses it.
  // Unsplit, the one pane (the active tab) occupies container 0.
  const Node* existing = isAlreadySplit ? TreeFor(mcv) : nullptr;
  Node tree = isAlreadySplit ? geometry::LiveTree(mcv, oldPanes) : MakeLeaf(0);
  const bool hasSplitActiveLeaf =
      SplitPane(tree, activeLeaf, direction, newLeaf);
  if (!hasSplitActiveLeaf) {
    SplitPane(tree, 0, direction, newLeaf);
  }
  Trees()[mcv] = tree;

  // Grow the data model. The feeder builds container[newLeaf], feeds it real
  // content and shows it; our layout detour positions it by the tree above.
  const bool hasGrown = split_data::GrowSplit(tsm);
  if (!hasGrown) {
    if (existing) {
      Trees()[mcv] = *existing;
    } else {
      ForgetTree(mcv);  // undo the speculative edit
    }
    Log("[dir-add] grow failed; tree reverted");
    return;
  }
  edge::InvalidateLayout(mcv);
  Log("[dir-add] dir=%d active_leaf=%d new_leaf=%d -> %d panes",
      static_cast<int>(direction), activeLeaf, newLeaf, oldPanes + 1);
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
// Containers after the closed one are re-fed one slot earlier, so each is first
// put back where its tab was on screen; the transition then carries every pane
// to its new place. Returns false to leave a two-pane split to Edge. UI thread.
inline bool ClosePane(void* mcv, int pane) {
  void* browserView = mcv ? edge::McvBrowserView(mcv) : nullptr;
  void* tsm = browserView ? edge::TabStripModelOf(edge::BrowserOf(browserView))
                          : nullptr;
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
  std::vector<Rect> onScreen;
  onScreen.reserve(static_cast<size_t>(memberCount));
  for (int i = 0; i < memberCount; i++) {
    onScreen.push_back(edge::ViewBounds(edge::McvContainerAt(mcv, i)));
  }

  const int remaining =
      split_data::CloseMember(tsm, closingContents, activeContents);
  if (remaining == 0) {
    return false;
  }
  RemovePane(tree, pane);
  Trees()[mcv] = tree;
  for (int i = pane; i < remaining; i++) {
    edge::SetViewBounds(edge::McvContainerAt(mcv, i), onScreen[i + 1]);
  }
  anim::Restart(mcv);
  edge::InvalidateLayout(mcv);
  edge::LayoutImmediately(mcv);
  Log("[close] pane %d closed; %d panes remain", pane, remaining);
  return true;
}

}  // namespace fixedge::split_ops
