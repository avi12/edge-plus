// pane_moves.h — a pane dragged and dropped: moved beside another or to an
// edge of the whole layout, its place traded with another's, and the swap a
// pane's ⇄ makes with its counterpart.
#pragma once
#include <cstddef>
#include <utility>
#include <vector>
#include "layout/pane_edits.h"
#include "layout/pane_normalize.h"
#include "layout/pane_tree.h"

namespace edgeplus {

// `to` is an insert-before index counted before `from` is removed.
inline void MoveChild(Node& parent, size_t from, size_t to) {
  const bool isMovingForward = from < to;
  if (isMovingForward) {
    to--;
  }
  Node child = std::move(parent.children[from]);
  const float weight = parent.weights[from];
  parent.children.erase(parent.children.begin() +
                        static_cast<std::ptrdiff_t>(from));
  parent.weights.erase(parent.weights.begin() +
                       static_cast<std::ptrdiff_t>(from));
  parent.children.insert(
      parent.children.begin() + static_cast<std::ptrdiff_t>(to),
      std::move(child));
  parent.weights.insert(
      parent.weights.begin() + static_cast<std::ptrdiff_t>(to), weight);
}

// Within the same split along that axis it is a pure reorder (the pane keeps
// its size). Returns false for a no-op.
inline bool MovePane(Node& root, int moving, int target, Direction direction) {
  const bool canMove = moving != target && !root.isLeaf;
  if (!canMove) {
    return false;
  }
  const Slot movingSlot = FindSlot(root, moving);
  const Slot targetSlot = FindSlot(root, target);
  const bool hasBothPanes = movingSlot.isFound && targetSlot.isFound;
  if (!hasBothPanes) {
    return false;
  }
  const bool isReorderWithinSplit =
      movingSlot.parent == targetSlot.parent &&
      movingSlot.parent->axis == AxisOf(direction);
  if (isReorderWithinSplit) {
    const size_t insertAt =
        IsBefore(direction) ? targetSlot.index : targetSlot.index + 1;
    const bool isAlreadyThere =
        insertAt == movingSlot.index || insertAt == movingSlot.index + 1;
    if (isAlreadyThere) {
      return false;
    }
    MoveChild(*movingSlot.parent, movingSlot.index, insertAt);
    return true;
  }
  Detach(root, moving);
  InsertBeside(root, target, direction, moving);
  Normalize(root);
  return true;
}

// `leaf` is not yet in the tree. A root split along that axis gains it as an
// end child at the average share; any other root is wrapped 50/50.
inline void InsertAtRootEdge(Node& root, int leaf, Direction direction) {
  const Axis axis = AxisOf(direction);
  const bool isLeadingEdge = IsBefore(direction);
  const bool isRootSplitAlongAxis = !root.isLeaf && root.axis == axis;
  if (isRootSplitAlongAxis) {
    const float averageWeight = Mean(root.weights);
    root.children.insert(
        isLeadingEdge ? root.children.begin() : root.children.end(),
        MakeLeaf(leaf));
    root.weights.insert(
        isLeadingEdge ? root.weights.begin() : root.weights.end(),
        averageWeight);
    FixWeights(root);
  } else {
    WrapWithLeaf(root, axis, isLeadingEdge, leaf);
  }
  Normalize(root);
}

// Returns false for a no-op.
inline bool MovePaneToRootEdge(Node& root, int moving, Direction direction) {
  const bool canMove = !root.isLeaf && Contains(root, moving);
  if (!canMove) {
    return false;
  }
  const Axis axis = AxisOf(direction);
  const bool isLeadingEdge = IsBefore(direction);
  const bool isRootAlongAxis = root.axis == axis;
  if (isRootAlongAxis) {
    const size_t edgeIndex = isLeadingEdge ? 0 : root.children.size() - 1;
    for (size_t i = 0; i < root.children.size(); i++) {
      if (!IsLeafFor(root.children[i], moving)) {
        continue;
      }
      const bool isAlreadyAtEdge = i == edgeIndex;
      if (isAlreadyAtEdge) {
        return false;
      }
      MoveChild(root, i, isLeadingEdge ? 0 : root.children.size());
      return true;
    }
  }
  // Detaching can collapse the root into a leaf, or into a split along the
  // other axis; InsertAtRootEdge handles both.
  Detach(root, moving);
  InsertAtRootEdge(root, moving, direction);
  return true;
}

// The sizes stay with the positions.
inline bool SwapLeaves(Node& node, int firstLeaf, int secondLeaf) {
  const bool isSamePane = firstLeaf == secondLeaf;
  if (isSamePane) {
    return false;
  }
  Node* firstNode = nullptr;
  Node* secondNode = nullptr;
  std::vector<Node*> stack{&node};
  while (!stack.empty()) {
    Node* current = stack.back();
    stack.pop_back();
    if (current->isLeaf) {
      const bool isFirstLeaf = current->leaf == firstLeaf;
      if (isFirstLeaf) {
        firstNode = current;
      }
      const bool isSecondLeaf = current->leaf == secondLeaf;
      if (isSecondLeaf) {
        secondNode = current;
      }
    } else {
      for (Node& child : current->children) {
        stack.push_back(&child);
      }
    }
  }
  const bool hasBothPanes = firstNode != nullptr && secondNode != nullptr;
  if (!hasBothPanes) {
    return false;
  }
  std::swap(firstNode->leaf, secondNode->leaf);
  return true;
}

inline bool SwapAxisIn(const Node& node, int leaf, Axis& out) {
  for (const Node& child : node.children) {
    if (IsLeafFor(child, leaf)) {
      out = node.axis;
      return true;
    }
    const bool isFoundBelow = !child.isLeaf && SwapAxisIn(child, leaf, out);
    if (isFoundBelow) {
      return true;
    }
  }
  return false;
}
// Which way the leaf's swap toggle faces.
[[nodiscard]] inline Axis SwapAxisOf(const Node& root, int leaf) {
  Axis axis = Axis::kHorizontal;
  SwapAxisIn(root, leaf, axis);
  return axis;
}

// The counterpart is the next sibling, else the previous.
inline bool SwapWithCounterpart(Node& node, int leaf) {
  for (size_t i = 0; i < node.children.size(); i++) {
    if (IsLeafFor(node.children[i], leaf)) {
      const bool hasNextSibling = i + 1 < node.children.size();
      if (hasNextSibling) {
        std::swap(node.children[i], node.children[i + 1]);
        return true;
      }
      const bool hasPreviousSibling = i > 0;
      if (hasPreviousSibling) {
        std::swap(node.children[i], node.children[i - 1]);
        return true;
      }
      return false;
    }
    const bool isSwappedBelow =
        !node.children[i].isLeaf && SwapWithCounterpart(node.children[i], leaf);
    if (isSwappedBelow) {
      return true;
    }
  }
  return false;
}

}  // namespace edgeplus
