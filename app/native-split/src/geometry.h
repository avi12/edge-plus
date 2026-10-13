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

// Build the out ProposedLayout for `mcv` across a `width`x`height` content box.
// Caller guarantees count > 2 and a bounded available size. Returns `out`.
inline void* TilePanes(void* mcv, void* out, int width, int height) {
  std::memset(out, 0, 0x20);  // ProposedLayout ctor only zeroes these 0x20 bytes
  edge::Field<int32_t>(out, off::PL_host_size + 0) = width;
  edge::Field<int32_t>(out, off::PL_host_size + 4) = height;

  const int count = static_cast<int>(edge::McvCount(mcv));
  const Axis axis = edge::McvOrientation(mcv) != 0 ? Axis::kVertical : Axis::kHorizontal;

  // Prefer the live tree; fall back to a flat equal split while none is set.
  const Node* live = TreeFor(mcv);
  const Node root = live ? *live : FlatTree(count, axis);

  std::vector<LeafRect> leaves;
  LayoutTree(root, Rect{0, 0, width, height}, /*gap=*/0, leaves);

  void* vec = reinterpret_cast<uint8_t*>(out) + off::PL_child_layouts;
  void** containers = edge::McvContainers(mcv);
  bool visible = true;
  int placed = 0;
  for (const LeafRect& lr : leaves) {
    if (lr.leaf < 0 || lr.leaf >= count) continue;  // guard tree/truth drift
    void* container = containers[lr.leaf];
    Rect rect = lr.rect;
    edge::EmplaceChildLayout(vec, &container, &visible, &rect);
    ++placed;
  }
  Log("[layout] tiled %d panes in %dx%d (%s)", placed, width, height,
      axis == Axis::kVertical ? "rows" : "cols");
  return out;
}

}  // namespace fixedge::geometry
