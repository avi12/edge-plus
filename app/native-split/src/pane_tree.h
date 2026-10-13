// pane_tree.h — the pure split-layout model. One concern, zero Edge/Win32 deps,
// so it is unit-testable in isolation (mirrors pade's pure reorder.ts). The tree
// is the single source of truth for where each pane sits; the geometry detour
// only translates its output into Edge ChildLayouts.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>
#include "geom.h"

namespace fixedge {

// A node splits its area among its children along one axis. Horizontal = children
// laid left->right; Vertical = stacked top->bottom. A leaf owns one container
// (by index into the MCV container vector).
enum class Axis : uint8_t { kHorizontal, kVertical };

struct Node {
  bool is_leaf = true;
  int leaf = -1;                 // container index, when a leaf
  Axis axis = Axis::kHorizontal; // split direction, when internal
  std::vector<Node> children;    // internal children, in order
  std::vector<float> weights;    // parallel to children; relative sizes
};

struct LeafRect {
  int leaf;
  Rect rect;
};

// Recursively place every leaf inside `bounds`, leaving `gap` px between siblings.
// Pure; early-returns on degenerate input. The last child absorbs rounding so the
// panes exactly tile the bounds with no seam drift.
inline void LayoutTree(const Node& node, const Rect& bounds, int gap,
                       std::vector<LeafRect>& out) {
  if (node.is_leaf) {
    out.push_back({node.leaf, bounds});
    return;
  }
  const size_t n = node.children.size();
  if (n == 0) return;

  float total = 0.f;
  for (size_t i = 0; i < n; ++i) total += (i < node.weights.size() ? node.weights[i] : 1.f);
  if (total <= 0.f) total = static_cast<float>(n);

  const bool horiz = node.axis == Axis::kHorizontal;
  const int span = (horiz ? bounds.w : bounds.h) - gap * static_cast<int>(n - 1);
  int cursor = horiz ? bounds.x : bounds.y;
  int used = 0;
  for (size_t i = 0; i < n; ++i) {
    const float w = (i < node.weights.size() ? node.weights[i] : 1.f);
    const int len = (i == n - 1) ? (span - used)
                                 : static_cast<int>(static_cast<float>(span) * (w / total));
    used += len;
    const Rect child = horiz ? Rect{cursor, bounds.y, len, bounds.h}
                             : Rect{bounds.x, cursor, bounds.w, len};
    LayoutTree(node.children[i], child, gap, out);
    cursor += len + gap;
  }
}

// A flat equal split of `count` leaves along `axis` — the proven behaviour and
// the fallback until the UI reshapes the tree.
inline Node FlatTree(int count, Axis axis) {
  Node root;
  root.is_leaf = false;
  root.axis = axis;
  for (int i = 0; i < count; ++i) {
    Node leaf;
    leaf.is_leaf = true;
    leaf.leaf = i;
    root.children.push_back(std::move(leaf));
    root.weights.push_back(1.f);
  }
  return root;
}

}  // namespace fixedge
