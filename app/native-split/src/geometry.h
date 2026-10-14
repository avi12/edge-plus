// geometry.h — the N-pane layout. One concern: translate the split tree's leaf
// rectangles into the ChildLayouts that Edge's layout applier consumes. The tree
// decides positions; this file only speaks Edge's ProposedLayout ABI.
#pragma once
#include <cstring>
#include <vector>
#include "edge.h"
#include "pane_tree.h"
#include "state.h"

namespace fixedge::geometry {

using namespace fixedge::edge155;  // off::, rva::

// Build the out ProposedLayout for `mcv` across a `width`x`height` content box,
// showing exactly `panes` of the MCV's containers (the live split's member
// count; 1 when collapsed). The container vector only grows, so any container
// beyond `panes` is hidden and given a zero rect rather than destroyed. Caller
// guarantees a bounded available size. Returns `out`.
inline void* TilePanes(void* mcv, void* out, int width, int height, int panes) {
  std::memset(out, 0, 0x20);  // ProposedLayout ctor only zeroes these 0x20 bytes
  edge::Field<int32_t>(out, off::PL_host_size + 0) = width;
  edge::Field<int32_t>(out, off::PL_host_size + 4) = height;

  const int count = static_cast<int>(edge::McvCount(mcv));
  if (panes < 1) panes = 1;
  if (panes > count) panes = count;
  const Axis axis = edge::McvOrientation(mcv) != 0 ? Axis::kVertical : Axis::kHorizontal;

  // Prefer the live tree; fall back to a flat equal split across `panes` leaves.
  const Node* live = TreeFor(mcv);
  const Node root = live ? *live : FlatTree(panes, axis);

  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, width, height}, /*gap=*/0, leaves);

  void* vec = reinterpret_cast<uint8_t*>(out) + off::PL_child_layouts;
  void** containers = edge::McvContainers(mcv);
  bool show = true, hide = false;
  int placed = 0;
  for (const LeafRect& lr : leaves) {
    if (lr.leaf < 0 || lr.leaf >= panes) continue;  // guard tree/truth drift
    void* container = containers[lr.leaf];
    edge::SetViewVisible(container, true);
    Rect rect = lr.rect;
    edge::EmplaceChildLayout(vec, &container, &show, &rect);
    ++placed;
  }
  // Surplus containers from a previously larger split: hide + zero them so they
  // leave no stale pixels (they are reused if the split grows again).
  for (int i = panes; i < count; ++i) {
    edge::SetViewVisible(containers[i], false);
    Rect zero{0, 0, 0, 0};
    edge::EmplaceChildLayout(vec, &containers[i], &hide, &zero);
  }
  Log("[layout] tiled %d/%d panes in %dx%d (%s)", placed, count, width, height,
      axis == Axis::kVertical ? "rows" : "cols");
  return out;
}

}  // namespace fixedge::geometry
