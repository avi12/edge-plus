// split_ops.h — the compound "add a pane in a direction" action. One concern:
// orchestrate the tree edit + container build + content feed + relayout that a
// directional-add menu command performs. UI thread only.
#pragma once
#include "edge.h"
#include "panes.h"
#include "tabs.h"
#include "state.h"
#include "pane_tree.h"
#include "split_data.h"

namespace fixedge::split_ops {

using namespace fixedge::edge155;

// Index of the active (primary) pane, else 0. The primary flag lives on each
// container's ContentsWebView (set when Edge makes that pane active).
inline int ActiveLeaf(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  for (int i = 0; i < count; ++i) {
    void* container = edge::McvContainerAt(mcv, i);
    if (container && edge::Field<uint8_t>(edge::ContainerWebView(container),
                                          off::CWV_primary_flag) == 1) {
      return i;
    }
  }
  return 0;
}

// Give a newly built pane content from a background tab (from the strip tail, so
// never the active/split tabs). Blank if none spare.
inline void FeedNewPane(void* mcv, int leaf) {
  void* browser_view = edge::McvBrowserView(mcv);
  void* tsm = edge::TabStripModelOf(edge::BrowserOf(browser_view));
  const int tab_count = tsm ? edge::TabCount(tsm) : 0;
  Log("[feed] FeedNewPane leaf=%d bv=%p tsm=%p tabs=%d", leaf, browser_view,
      tsm, tab_count);
  for (int t = tab_count - 1; t >= 0; --t) {
    if (tabs::FillPane(browser_view, edge::McvContainerAt(mcv, leaf), t)) {
      Log("[feed] fed leaf %d from tab %d", leaf, t);
      return;
    }
  }
  Log("[feed] no background tab fed into leaf %d", leaf);
}

// Add one pane adjacent to the active pane, in `dir`, as a real split-tree edit.
// The data model grows by one member (GrowSplit, which appends it as the last
// container), and the tree records where that container sits relative to the
// active pane. The layout detour then tiles containers per the tree. UI thread.
inline void AddPane(void* mcv, Direction dir) {
  if (!mcv) {
    return;
  }
  void* bv = edge::McvBrowserView(mcv);
  void* tsm = bv ? edge::TabStripModelOf(edge::BrowserOf(bv)) : nullptr;
  if (!tsm) {
    return;
  }
  void* active = edge::ActiveTab(tsm);
  const bool was_split = active && edge::TabIsSplit(active);
  const int old_panes = was_split ? edge::McvActiveSplitPanes(mcv) : 1;
  const int active_leaf = was_split ? ActiveLeaf(mcv) : 0;
  const int new_leaf =
      old_panes;  // GrowSplit appends the new tab as the last container

  // Reshape the tree first so the relayout the grow triggers already uses it.
  const Node* existing = was_split ? TreeFor(mcv) : nullptr;
  Node tree;
  if (existing) {
    tree = *existing;
  } else if (was_split) {
    tree = FlatTree(old_panes, edge::McvOrientation(mcv) ? Axis::kVertical
                                                         : Axis::kHorizontal);
  } else {
    tree = MakeLeaf(0);  // one pane (the active tab) occupies container 0
  }
  if (!SplitPane(tree, active_leaf, dir, new_leaf)) {
    SplitPane(tree, 0, dir, new_leaf);
  }
  Trees()[mcv] = tree;

  // Grow the data model. The feeder builds container[new_leaf], feeds it real
  // content and shows it; our layout detour positions it by the tree above.
  if (!split_data::GrowSplit(tsm)) {
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
      static_cast<int>(dir), active_leaf, new_leaf, old_panes + 1);
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
  const int active = ActiveLeaf(mcv);
  Node tree = *existing;
  if (SwapWithCounterpart(tree, active)) {
    Trees()[mcv] = tree;
    edge::InvalidateLayout(mcv);
    edge::LayoutImmediately(mcv);  // no data-model change, so force the re-tile
    Log("[swap] swapped pane %d with its counterpart", active);
  } else {
    Log("[swap] pane %d has no counterpart", active);
  }
}

}  // namespace fixedge::split_ops
