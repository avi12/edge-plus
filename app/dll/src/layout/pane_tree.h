// pane_tree.h — the pure split-layout model: the single source of truth for where
// each pane sits; the geometry detour only translates it into Edge ChildLayouts.
// The tree itself and how to read it: its nodes and leaves, a direction, a
// pane's slot and the panes in it. Laying it out, editing it and its seams are
// the other pane_*.h headers.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "edge/geom.h"

namespace edgeplus {

enum class Axis : uint8_t {
  kHorizontal,
  kVertical
};

// A leaf id no container has: an unset leaf, a drop with no target pane.
inline constexpr int kNoLeaf = -1;

struct Node {
  bool isLeaf = true;
  int leaf = kNoLeaf;  // the index into the MCV's containers, when a leaf
  Axis axis = Axis::kHorizontal;
  std::vector<Node> children;
  std::vector<float> weights;  // parallel to children; relative sizes
};

struct LeafRect {
  int leaf = kNoLeaf;
  Rect rect;
};

[[nodiscard]] inline Node MakeLeaf(int leaf) {
  Node node;
  node.isLeaf = true;
  node.leaf = leaf;
  return node;
}

[[nodiscard]] inline Node FlatTree(int count, Axis axis) {
  Node root;
  root.isLeaf = false;
  root.axis = axis;
  for (int i = 0; i < count; i++) {
    root.children.push_back(MakeLeaf(i));
    root.weights.push_back(1.f);
  }
  return root;
}

// A two-pane split as Edge lays it out itself: one axis, and its members in
// order along it.
struct Pair {
  Axis axis = Axis::kHorizontal;
  bool isSecondLeafFirst = false;  // leaf 1 left of (or above) leaf 0
};

// Whether `tree` is two panes side by side or one above the other, and in
// which order; false for any other tree.
[[nodiscard]] inline bool ReadPair(const Node& tree, Pair& pair) {
  const bool isTwoLeaves = !tree.isLeaf && tree.children.size() == 2 &&
                           tree.children[0].isLeaf && tree.children[1].isLeaf;
  if (!isTwoLeaves) {
    return false;
  }
  pair.axis = tree.axis;
  pair.isSecondLeafFirst = tree.children[0].leaf == 1;
  return true;
}

enum class Direction : uint8_t {
  kUp,
  kDown,
  kLeft,
  kRight
};

[[nodiscard]] inline constexpr Axis AxisOf(Direction direction) noexcept {
  const bool isVertical =
      direction == Direction::kUp || direction == Direction::kDown;
  return isVertical ? Axis::kVertical : Axis::kHorizontal;
}
[[nodiscard]] inline constexpr bool IsBefore(Direction direction) noexcept {
  return direction == Direction::kUp || direction == Direction::kLeft;
}

[[nodiscard]] inline bool IsLeafFor(const Node& node, int leaf) {
  return node.isLeaf && node.leaf == leaf;
}

// Leaves are found by id, never by a stored path, so a lookup after an edit is
// always fresh.
struct Slot {
  Node* parent = nullptr;
  size_t index = 0;
  bool isFound = false;
};
[[nodiscard]] inline Slot FindSlot(Node& node, int leaf) {
  for (size_t i = 0; i < node.children.size(); i++) {
    Node& child = node.children[i];
    if (IsLeafFor(child, leaf)) {
      return {&node, i, true};
    }
    if (!child.isLeaf) {
      const Slot slot = FindSlot(child, leaf);
      if (slot.isFound) {
        return slot;
      }
    }
  }
  return {};
}
[[nodiscard]] inline bool Contains(const Node& node, int leaf) {
  if (node.isLeaf) {
    return node.leaf == leaf;
  }
  for (const Node& child : node.children) {
    if (Contains(child, leaf)) {
      return true;
    }
  }
  return false;
}

inline void CollectLeaves(const Node& node, std::vector<int>& out) {
  if (node.isLeaf) {
    out.push_back(node.leaf);
    return;
  }
  for (const Node& child : node.children) {
    CollectLeaves(child, out);
  }
}

[[nodiscard]] inline int LeafCount(const Node& node) {
  if (node.isLeaf) {
    return 1;
  }
  int count = 0;
  for (const Node& child : node.children) {
    count += LeafCount(child);
  }
  return count;
}

[[nodiscard]] inline Node* NodeAt(Node& root, const std::vector<int>& path) {
  Node* node = &root;
  for (const int childIndex : path) {
    const bool hasChild =
        !node->isLeaf && childIndex >= 0 &&
        static_cast<size_t>(childIndex) < node->children.size();
    if (!hasChild) {
      return nullptr;
    }
    node = &node->children[static_cast<size_t>(childIndex)];
  }
  return node;
}

}  // namespace edgeplus
