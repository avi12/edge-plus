// pane_edits.h — a pane added or removed: inserted beside another, split off
// one, detached, and the panes renumbered after one leaves or comes back.
#pragma once
#include <cstddef>
#include <utility>
#include "layout/pane_normalize.h"
#include "layout/pane_tree.h"

namespace edgeplus {

[[nodiscard]] inline Node MakeSplit(Axis axis, Node first, Node second) {
  Node split;
  split.isLeaf = false;
  split.axis = axis;
  split.children.push_back(std::move(first));
  split.children.push_back(std::move(second));
  split.weights = {1.f, 1.f};
  return split;
}

inline void WrapWithLeaf(Node& node, Axis axis, bool isLeafFirst, int leaf) {
  Node old = std::move(node);
  node = isLeafFirst ? MakeSplit(axis, MakeLeaf(leaf), std::move(old))
                     : MakeSplit(axis, std::move(old), MakeLeaf(leaf));
}

// If the target's split already runs along that axis the leaf joins it: an equal
// row stays equal, a resized row only gives up half of the target. Otherwise the
// target is wrapped in a new 50/50 split that keeps the target's slot.
inline bool InsertBeside(Node& root, int target, Direction direction,
                         int leaf) {
  const Axis axis = AxisOf(direction);
  const bool isInsertedBefore = IsBefore(direction);
  if (root.isLeaf) {
    const bool isRootTheTarget = root.leaf == target;
    if (!isRootTheTarget) {
      return false;
    }
    WrapWithLeaf(root, axis, isInsertedBefore, leaf);
    return true;
  }
  const Slot slot = FindSlot(root, target);
  if (!slot.isFound) {
    return false;
  }
  Node& parent = *slot.parent;
  const bool isParentAlongAxis = parent.axis == axis;
  if (isParentAlongAxis) {
    const size_t insertAt = isInsertedBefore ? slot.index : slot.index + 1;
    float newWeight = 0.f;
    const bool isEqualRow = AllEqual(parent.weights);
    if (isEqualRow) {
      newWeight = Mean(parent.weights);
    } else {
      newWeight = parent.weights[slot.index] / 2.f;
      parent.weights[slot.index] -= newWeight;
    }
    parent.children.insert(
        parent.children.begin() + static_cast<std::ptrdiff_t>(insertAt),
        MakeLeaf(leaf));
    parent.weights.insert(
        parent.weights.begin() + static_cast<std::ptrdiff_t>(insertAt),
        newWeight);
    FixWeights(parent);
  } else {
    WrapWithLeaf(parent.children[slot.index], axis, isInsertedBefore, leaf);
  }
  return true;
}

inline bool SplitPane(Node& root, int targetLeaf, Direction direction,
                      int newLeaf) {
  const bool isInserted = InsertBeside(root, targetLeaf, direction, newLeaf);
  if (!isInserted) {
    return false;
  }
  Normalize(root);
  return true;
}

// The weight is dropped so the siblings grow in proportion. Refuses to remove
// the last pane.
inline bool Detach(Node& root, int leaf) {
  if (root.isLeaf) {
    return false;
  }
  const Slot slot = FindSlot(root, leaf);
  if (!slot.isFound) {
    return false;
  }
  slot.parent->children.erase(slot.parent->children.begin() +
                              static_cast<std::ptrdiff_t>(slot.index));
  slot.parent->weights.erase(slot.parent->weights.begin() +
                             static_cast<std::ptrdiff_t>(slot.index));
  Normalize(root);
  return true;
}

// The containers after a closed pane's are re-fed one slot earlier, and the tree
// follows its tabs.
inline void RenumberAfterRemoval(Node& node, int removed) {
  if (node.isLeaf) {
    const bool isAfterRemoved = node.leaf > removed;
    if (isAfterRemoved) {
      node.leaf--;
    }
    return;
  }
  for (Node& child : node.children) {
    RenumberAfterRemoval(child, removed);
  }
}

// The same layout once `leaf` has left the split and come back as its newest
// pane: the panes after it are re-fed one slot earlier and it takes the last
// slot, `newest`.
inline void RenumberAsNewest(Node& node, int leaf, int newest) {
  if (node.isLeaf) {
    if (node.leaf == leaf) {
      node.leaf = newest;
    } else if (node.leaf > leaf) {
      node.leaf--;
    }
    return;
  }
  for (Node& child : node.children) {
    RenumberAsNewest(child, leaf, newest);
  }
}

inline bool RemovePane(Node& root, int leaf) {
  if (!Detach(root, leaf)) {
    return false;
  }
  RenumberAfterRemoval(root, leaf);
  return true;
}

}  // namespace edgeplus
