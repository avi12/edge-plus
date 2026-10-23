// geometry.h — the N-pane layout: the split tree's leaf rectangles as the
// ChildLayouts of Edge's ProposedLayout.
#pragma once
#include <algorithm>
#include <cstring>
#include <vector>
#include "edge.h"
#include "pane_tree.h"
#include "state.h"
#include "anim.h"
#include "dividers.h"

namespace fixedge::geometry {

// SplitTabLayout: 0 is one axis, any other value the other.
inline Axis SplitAxis(void* mcv) {
  return edge::McvOrientation(mcv) != 0 ? Axis::kVertical : Axis::kHorizontal;
}

// The live tree only when it tiles that many panes: a plain tab or another split
// shown in the same MCV must not take the grown split's geometry (a plain tab
// drew in the old first pane's box).
inline Node LiveTree(void* mcv, int panes) {
  const Node* live = TreeFor(mcv);
  const bool isLiveForPanes = live && LeafCount(*live) == panes;
  if (isLiveForPanes) {
    return *live;
  }
  return FlatTree(panes, SplitAxis(mcv));
}

inline Rect SplitArea(void* mcv) {
  const Rect bounds = edge::ViewBounds(mcv);
  return Rect{0, 0, bounds.width, bounds.height};
}

// The bytes a ProposedLayout occupies (its constructor only zeroes these).
inline constexpr size_t kProposedLayoutBytes = 0x20;

// `panes` is the live split's member count (1 when collapsed). The container
// vector only grows, so surplus containers are hidden with a zero rect rather
// than destroyed. Caller guarantees a bounded available size. `snap` (a window
// resize) skips easing.
inline void* TilePanes(void* mcv, void* out, int width, int height, int panes,
                       bool snap) {
  std::memset(out, 0, kProposedLayoutBytes);
  edge::Field<int32_t>(out, edge155::off::PL_host_size + 0) = width;
  edge::Field<int32_t>(out, edge155::off::PL_host_size + 4) = height;

  const int count = static_cast<int>(edge::McvCount(mcv));
  const int shownPanes = std::min(std::max(panes, 1), count);
  const Axis axis = SplitAxis(mcv);
  const Node root = LiveTree(mcv, shownPanes);

  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, width, height}, kPaneGap, leaves);

  // Guard tree/truth drift: only a leaf naming a shown container is placed.
  std::vector<LeafRect> placed;
  for (const LeafRect& leafRect : leaves) {
    const bool isShownLeaf = leafRect.leaf >= 0 && leafRect.leaf < shownPanes;
    if (isShownLeaf) {
      placed.push_back(leafRect);
    }
  }
  anim::Apply(mcv, placed, snap);

  void* childLayouts =
      reinterpret_cast<uint8_t*>(out) + edge155::off::PL_child_layouts;
  void** containers = edge::McvContainers(mcv);
  for (const LeafRect& leafRect : placed) {
    void* container = containers[leafRect.leaf];
    edge::SetViewVisible(container, true);
    bool isVisible = true;
    Rect rect = leafRect.rect;
    edge::EmplaceChildLayout(childLayouts, &container, &isVisible, &rect);
  }
  // Zeroed too, so they leave no stale pixels.
  for (int i = shownPanes; i < count; i++) {
    edge::SetViewVisible(containers[i], false);
    bool isVisible = false;
    Rect zeroRect{0, 0, 0, 0};
    edge::EmplaceChildLayout(childLayouts, &containers[i], &isVisible,
                             &zeroRect);
  }
  dividers::Place(mcv, root, Rect{0, 0, width, height}, childLayouts);
  Log("[layout] tiled %zu/%d panes in %dx%d (%s)", placed.size(), count, width,
      height, axis == Axis::kVertical ? "rows" : "cols");
  return out;
}

// Before the layout hook comes off. Edge's GetViewSizes CHECKs that panes 0 and
// 1 are both visible whenever its divider handle is, and TilePanes leaves pane
// 1 hidden with the handle visible on a grown split collapsed to one pane: the
// next stock layout (e.g. a theme change's frame relayout) crashed Edge.
inline void RestoreStockVisibility(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  const bool isSplitShown = edge::McvActiveSplitPanes(mcv) >= 2;
  void* const* containers = edge::McvContainers(mcv);
  for (int i = 0; i < count; i++) {
    const bool isShownPane = i == 0 || (i == 1 && isSplitShown);
    edge::SetViewVisible(containers[i], isShownPane);
  }
  if (void* edgeHandle = edge::McvResizeArea(mcv)) {
    edge::SetViewVisible(edgeHandle, isSplitShown);
  }
  edge::InvalidateLayout(mcv);
}

}  // namespace fixedge::geometry
