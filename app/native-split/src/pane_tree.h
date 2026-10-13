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

// --- pure tree edits (the Arc behaviour: directional add, swap) ---------------

// The four directions a pane can be grown in. Up/Down split along the vertical
// axis (stacked); Left/Right along the horizontal. Up/Left put the new pane
// before the target; Down/Right after it.
enum class Direction : uint8_t { kUp, kDown, kLeft, kRight };

inline Axis AxisOf(Direction d) {
  return (d == Direction::kUp || d == Direction::kDown) ? Axis::kVertical : Axis::kHorizontal;
}
inline bool IsBefore(Direction d) { return d == Direction::kUp || d == Direction::kLeft; }

inline Node MakeLeaf(int leaf) {
  Node n;
  n.is_leaf = true;
  n.leaf = leaf;
  return n;
}

// Insert `new_leaf` adjacent to `target_leaf` in `dir`. If the target already
// lives in a split along the needed axis, the new pane joins that split as an
// equal sibling (growing a row/column); otherwise the target leaf becomes a
// 2-way split of the two. Returns true if the target was found. Pure.
inline bool SplitLeaf(Node& node, int target_leaf, Direction dir, int new_leaf) {
  const Axis want = AxisOf(dir);
  const bool before = IsBefore(dir);
  for (size_t i = 0; i < node.children.size(); ++i) {
    Node& child = node.children[i];
    if (child.is_leaf && child.leaf == target_leaf) {
      if (node.axis == want) {  // join the existing split
        const size_t at = before ? i : i + 1;
        node.children.insert(node.children.begin() + at, MakeLeaf(new_leaf));
        node.weights.insert(node.weights.begin() + at, 1.f);
      } else {  // turn this leaf into a 2-way split along `want`
        Node split;
        split.is_leaf = false;
        split.axis = want;
        Node keep = MakeLeaf(target_leaf);
        Node add = MakeLeaf(new_leaf);
        if (before) {
          split.children = {std::move(add), std::move(keep)};
        } else {
          split.children = {std::move(keep), std::move(add)};
        }
        split.weights = {1.f, 1.f};
        child = std::move(split);
      }
      return true;
    }
    if (!child.is_leaf && SplitLeaf(child, target_leaf, dir, new_leaf)) return true;
  }
  return false;
}

// Top-level split that also handles a single-leaf root.
inline bool SplitPane(Node& root, int target_leaf, Direction dir, int new_leaf) {
  if (root.is_leaf) {
    if (root.leaf != target_leaf) return false;
    const bool before = IsBefore(dir);
    Node keep = MakeLeaf(target_leaf);
    Node add = MakeLeaf(new_leaf);
    root.is_leaf = false;
    root.leaf = -1;
    root.axis = AxisOf(dir);
    root.children = before ? std::vector<Node>{std::move(add), std::move(keep)}
                           : std::vector<Node>{std::move(keep), std::move(add)};
    root.weights = {1.f, 1.f};
    return true;
  }
  return SplitLeaf(root, target_leaf, dir, new_leaf);
}

// The axis of the split a leaf belongs to — i.e. which way its swap toggle faces
// (vertical parent → top/bottom swap, horizontal → left/right). Searches for the
// parent that directly holds the leaf. Returns kHorizontal if not found.
inline bool SwapAxisIn(const Node& node, int leaf, Axis& out) {
  for (const Node& child : node.children) {
    if (child.is_leaf && child.leaf == leaf) {
      out = node.axis;
      return true;
    }
    if (!child.is_leaf && SwapAxisIn(child, leaf, out)) return true;
  }
  return false;
}
inline Axis SwapAxisOf(const Node& root, int leaf) {
  Axis a = Axis::kHorizontal;
  SwapAxisIn(root, leaf, a);
  return a;
}

// Swap a leaf with its adjacent sibling along the parent's axis (its "counterpart"
// in the spec). Returns true if a swap happened. Pure — operates on the tree's
// leaf indices, so the caller does not move any Edge object.
inline bool SwapWithCounterpart(Node& node, int leaf) {
  for (size_t i = 0; i < node.children.size(); ++i) {
    if (node.children[i].is_leaf && node.children[i].leaf == leaf) {
      // Prefer the next sibling, else the previous — the counterpart on the axis.
      if (i + 1 < node.children.size()) {
        std::swap(node.children[i], node.children[i + 1]);
        return true;
      }
      if (i > 0) {
        std::swap(node.children[i], node.children[i - 1]);
        return true;
      }
      return false;
    }
    if (!node.children[i].is_leaf && SwapWithCounterpart(node.children[i], leaf)) return true;
  }
  return false;
}

// Collect leaf indices left-to-right / top-to-bottom. Pure.
inline void CollectLeaves(const Node& node, std::vector<int>& out) {
  if (node.is_leaf) {
    out.push_back(node.leaf);
    return;
  }
  for (const Node& child : node.children) CollectLeaves(child, out);
}

}  // namespace fixedge
