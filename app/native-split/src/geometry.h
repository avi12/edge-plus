// geometry.h — the N-pane layout. One concern: translate the split tree's leaf
// rectangles into the ChildLayouts that Edge's layout applier consumes. The tree
// decides positions; this file only speaks Edge's ProposedLayout ABI.
#pragma once
#include <cstring>
#include <vector>
#include "edge.h"
#include "pane_tree.h"
#include "state.h"
#include "anim.h"

namespace fixedge::geometry {

using namespace fixedge::edge155;  // off::, rva::

// The tree `mcv`'s `panes` are tiled by: the live one, else a flat equal split
// along the split's orientation. The one rule both the layout and a drag use.
inline Node LiveTree(void* mcv, int panes) {
  if (const Node* live = TreeFor(mcv)) {
    return *live;
  }
  return FlatTree(panes, edge::McvOrientation(mcv) != 0 ? Axis::kVertical
                                                        : Axis::kHorizontal);
}

// The split's content box in its own coordinates (what TilePanes tiles).
inline Rect SplitArea(void* mcv) {
  const Rect b = edge::ViewBounds(mcv);
  return Rect{0, 0, b.width, b.height};
}

// Build the out ProposedLayout for `mcv` across a `width`x`height` content box,
// showing exactly `panes` of the MCV's containers (the live split's member
// count; 1 when collapsed). The container vector only grows, so any container
// beyond `panes` is hidden and given a zero rect rather than destroyed. Caller
// guarantees a bounded available size. Pane moves are eased by anim::Apply
// unless `snap` (a window resize). Returns `out`.
inline void* TilePanes(void* mcv, void* out, int width, int height, int panes,
                       bool snap) {
  std::memset(out, 0,
              0x20);  // ProposedLayout ctor only zeroes these 0x20 bytes
  edge::Field<int32_t>(out, off::PL_host_size + 0) = width;
  edge::Field<int32_t>(out, off::PL_host_size + 4) = height;

  const int count = static_cast<int>(edge::McvCount(mcv));
  if (panes < 1) {
    panes = 1;
  }
  if (panes > count) {
    panes = count;
  }
  const Axis axis =
      edge::McvOrientation(mcv) != 0 ? Axis::kVertical : Axis::kHorizontal;
  const Node root = LiveTree(mcv, panes);

  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, width, height}, /*gap=*/0, leaves);

  std::vector<LeafRect> placed;
  for (const LeafRect& lr : leaves) {
    if (lr.leaf >= 0 && lr.leaf < panes) {
      placed.push_back(lr);  // guard tree/truth drift
    }
  }
  anim::Apply(mcv, placed, snap);  // targets -> this frame's rects

  void* vec = reinterpret_cast<uint8_t*>(out) + off::PL_child_layouts;
  void** containers = edge::McvContainers(mcv);
  bool show = true, hide = false;
  for (const LeafRect& lr : placed) {
    void* container = containers[lr.leaf];
    edge::SetViewVisible(container, true);
    Rect rect = lr.rect;
    edge::EmplaceChildLayout(vec, &container, &show, &rect);
  }
  // Surplus containers from a previously larger split: hide + zero them so they
  // leave no stale pixels (they are reused if the split grows again).
  for (int i = panes; i < count; ++i) {
    edge::SetViewVisible(containers[i], false);
    Rect zero{0, 0, 0, 0};
    edge::EmplaceChildLayout(vec, &containers[i], &hide, &zero);
  }
  Log("[layout] tiled %zu/%d panes in %dx%d (%s)", placed.size(), count, width,
      height, axis == Axis::kVertical ? "rows" : "cols");
  return out;
}

}  // namespace fixedge::geometry
