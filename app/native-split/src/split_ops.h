// split_ops.h — the compound "add a pane in a direction" action. One concern:
// orchestrate the tree edit + container build + content feed + relayout that a
// directional-add menu command performs. UI thread only.
#pragma once
#include "edge.h"
#include "panes.h"
#include "tabs.h"
#include "state.h"
#include "pane_tree.h"

namespace fixedge::split_ops {

using namespace fixedge::edge155;

// Index of the active (primary) pane, else 0. The primary flag lives on each
// container's ContentsWebView (set when Edge makes that pane active).
inline int ActiveLeaf(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  for (int i = 0; i < count; ++i) {
    void* container = edge::McvContainerAt(mcv, i);
    if (container && edge::Field<uint8_t>(edge::ContainerWebView(container), off::CWV_primary_flag) == 1) {
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
  Log("[feed] FeedNewPane leaf=%d bv=%p tsm=%p tabs=%d", leaf, browser_view, tsm, tab_count);
  for (int t = tab_count - 1; t >= 0; --t) {
    if (tabs::FillPane(browser_view, edge::McvContainerAt(mcv, leaf), t)) {
      Log("[feed] fed leaf %d from tab %d", leaf, t);
      return;
    }
  }
  Log("[feed] no background tab fed into leaf %d", leaf);
}

// Add one pane adjacent to the active pane, in `dir`. Mutates the split tree,
// builds the container, feeds it, and relays out. No-op without a live split.
inline void AddPane(void* mcv, Direction dir) {
  if (!mcv) return;
  const int count = static_cast<int>(edge::McvCount(mcv));
  if (count < 1) return;

  const int active = ActiveLeaf(mcv);
  const Node* existing = TreeFor(mcv);
  Node tree = existing ? *existing : FlatTree(count, Axis::kHorizontal);
  const int new_leaf = count;
  if (!SplitPane(tree, active, dir, new_leaf)) {
    SplitPane(tree, 0, dir, new_leaf);  // fall back to the first leaf
  }
  Trees()[mcv] = tree;

  if (!panes::AddContainer(mcv)) return;
  FeedNewPane(mcv, new_leaf);
  edge::InvalidateLayout(mcv);
  Log("[menu] add pane dir=%d active=%d -> count=%d", static_cast<int>(dir), active,
      static_cast<int>(edge::McvCount(mcv)));
}

}  // namespace fixedge::split_ops
