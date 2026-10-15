// geometry.h — the N-pane layout. One concern: translate the split tree's leaf
// rectangles into the ChildLayouts that Edge's layout applier consumes. The tree
// decides positions; this file only speaks Edge's ProposedLayout ABI.
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

using namespace fixedge::edge155;  // off::, rva::

// The split's orientation as a tree axis (SplitTabLayout: 0 is one axis, any
// other value the other).
inline Axis SplitAxis(void* mcv) {
  return edge::McvOrientation(mcv) != 0 ? Axis::kVertical : Axis::kHorizontal;
}

// The tree `mcv`'s `panes` are tiled by: the live one when it tiles that many
// panes, else a flat equal split along the split's orientation. The one rule
// the layout, a drag and a directional add all use. The live tree is the
// MCV's, not a tab's: a plain tab (one pane) or another split shown in the
// same window tiles a different count and must not take the grown split's
// geometry (a plain tab drew in the old first pane's box).
inline Node LiveTree(void* mcv, int panes) {
  const Node* live = TreeFor(mcv);
  const bool isLiveForPanes = live && LeafCount(*live) == panes;
  if (isLiveForPanes) {
    return *live;
  }
  return FlatTree(panes, SplitAxis(mcv));
}

// The split's content box in its own coordinates (what TilePanes tiles).
inline Rect SplitArea(void* mcv) {
  const Rect bounds = edge::ViewBounds(mcv);
  return Rect{0, 0, bounds.width, bounds.height};
}

// Build the out ProposedLayout for `mcv` across a `width`x`height` content box,
// showing exactly `panes` of the MCV's containers (the live split's member
// count; 1 when collapsed). The container vector only grows, so any container
// beyond `panes` is hidden and given a zero rect rather than destroyed. Caller
// guarantees a bounded available size. Pane moves are eased by anim::Apply
// unless `snap` (a window resize). Returns `out`.
inline void* TilePanes(void* mcv, void* out, int width, int height, int panes,
                       bool snap) {
  // The ProposedLayout ctor only zeroes these 0x20 bytes.
  std::memset(out, 0, 0x20);
  edge::Field<int32_t>(out, off::PL_host_size + 0) = width;
  edge::Field<int32_t>(out, off::PL_host_size + 4) = height;

  const int count = static_cast<int>(edge::McvCount(mcv));
  // At least one pane, and never more than there are containers.
  const int shownPanes = std::min(std::max(panes, 1), count);
  const Axis axis = SplitAxis(mcv);
  const Node root = LiveTree(mcv, shownPanes);

  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, width, height}, /*gap=*/0, leaves);

  // Guard tree/truth drift: only a leaf naming a shown container is placed.
  std::vector<LeafRect> placed;
  for (const LeafRect& leafRect : leaves) {
    const bool isShownLeaf = leafRect.leaf >= 0 && leafRect.leaf < shownPanes;
    if (isShownLeaf) {
      placed.push_back(leafRect);
    }
  }
  anim::Apply(mcv, placed, snap);  // targets -> this frame's rects

  void* childLayouts = reinterpret_cast<uint8_t*>(out) + off::PL_child_layouts;
  void** containers = edge::McvContainers(mcv);
  for (const LeafRect& leafRect : placed) {
    void* container = containers[leafRect.leaf];
    edge::SetViewVisible(container, true);
    bool isVisible = true;
    Rect rect = leafRect.rect;
    edge::EmplaceChildLayout(childLayouts, &container, &isVisible, &rect);
  }
  // Surplus containers from a previously larger split: hide + zero them so they
  // leave no stale pixels (they are reused if the split grows again).
  for (int i = shownPanes; i < count; i++) {
    edge::SetViewVisible(containers[i], false);
    bool isVisible = false;
    Rect zeroRect{0, 0, 0, 0};
    edge::EmplaceChildLayout(childLayouts, &containers[i], &isVisible,
                             &zeroRect);
  }
  // The dividers sit over the seams of the target layout.
  dividers::Place(mcv, root, Rect{0, 0, width, height}, childLayouts);
  Log("[layout] tiled %zu/%d panes in %dx%d (%s)", placed.size(), count, width,
      height, axis == Axis::kVertical ? "rows" : "cols");
  return out;
}

}  // namespace fixedge::geometry
