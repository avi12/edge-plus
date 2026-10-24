// pane_tree.h — the pure split-layout model: the single source of truth for where
// each pane sits; the geometry detour only translates it into Edge ChildLayouts.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include "geom.h"

namespace fixedge {

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

// Capped at an equal share of the span, so a small window shrinks panes evenly
// instead of overflowing.
inline constexpr int kDesiredMinPane = 64;

// The room Edge's own split leaves its resize handle. No pane may cover a
// divider, since a page's native window takes every click over it.
inline constexpr int kPaneGap = 8;

struct SpanShare {
  std::vector<bool> isPinned;  // parallel to the weights
  float freeWeight = 0.f;      // the unpinned panes' total weight
  int freeSpan = 0;            // the length the unpinned panes share
};

[[nodiscard]] inline constexpr float FreeLength(const SpanShare& share,
                                                float weight) noexcept {
  return static_cast<float>(share.freeSpan) * (weight / share.freeWeight);
}

// Water-fill: pin to `minLength` only the panes whose proportional length
// would fall below it; the rest keep their exact ratio of what the pinned ones
// leave. Each round can push another pane under, so it repeats until none is.
[[nodiscard]] inline SpanShare WaterFill(std::span<const float> weights,
                                         int span, int minLength) {
  const size_t count = weights.size();
  SpanShare share;
  share.isPinned.assign(count, false);
  while (true) {
    share.freeWeight = 0.f;
    int pinnedCount = 0;
    for (size_t i = 0; i < count; i++) {
      if (share.isPinned[i]) {
        pinnedCount++;
      } else {
        share.freeWeight += weights[i];
      }
    }
    share.freeSpan = span - minLength * pinnedCount;
    bool hasNewPin = false;
    const bool hasFreeWeight = share.freeWeight > 0.f;
    if (hasFreeWeight) {
      for (size_t i = 0; i < count; i++) {
        const bool shouldPin =
            !share.isPinned[i] &&
            FreeLength(share, weights[i]) < static_cast<float>(minLength);
        if (shouldPin) {
          share.isPinned[i] = true;
          hasNewPin = true;
        }
      }
    }
    if (!hasNewPin) {
      return share;
    }
  }
}

[[nodiscard]] inline std::vector<Rect> ChildRects(const Node& node,
                                                  const Rect& bounds, int gap) {
  std::vector<Rect> rects;
  const int childCount = static_cast<int>(node.children.size());
  if (node.isLeaf || childCount == 0) {
    return rects;
  }
  rects.reserve(node.children.size());

  std::vector<float> weights(childCount);
  for (int i = 0; i < childCount; i++) {
    const bool hasWeight = static_cast<size_t>(i) < node.weights.size();
    weights[i] = hasWeight ? node.weights[i] : 1.f;
  }

  const bool isHorizontal = node.axis == Axis::kHorizontal;
  const int span = std::max(0, (isHorizontal ? bounds.width : bounds.height) -
                                   gap * (childCount - 1));
  const int minLength = std::min(kDesiredMinPane, span / childCount);
  const SpanShare share = WaterFill(weights, span, minLength);
  const bool hasFreeWeight = share.freeWeight > 0.f;

  int cursor = isHorizontal ? bounds.x : bounds.y;
  int used = 0;
  for (int i = 0; i < childCount; i++) {
    int length = 0;
    const bool isLastChild = i == childCount - 1;
    if (isLastChild) {
      length = span - used;  // absorbs rounding, so the panes tile exactly
    } else if (share.isPinned[i]) {
      length = minLength;
    } else {
      length =
          hasFreeWeight
              ? static_cast<int>(std::lround(FreeLength(share, weights[i])))
              : minLength;
    }
    used += length;
    rects.push_back(isHorizontal
                        ? Rect{cursor, bounds.y, length, bounds.height}
                        : Rect{bounds.x, cursor, bounds.width, length});
    cursor += length + gap;
  }
  return rects;
}

inline void LayoutTree(const Node& node, const Rect& bounds, int gap,
                       std::vector<LeafRect>& out) {
  if (node.isLeaf) {
    out.push_back({node.leaf, bounds});
    return;
  }
  const std::vector<Rect> rects = ChildRects(node, bounds, gap);
  for (size_t i = 0; i < rects.size(); i++) {
    LayoutTree(node.children[i], rects[i], gap, out);
  }
}

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

// Panes sit kPaneGap apart, so a direct neighbour's gap is positive; this
// tolerance only absorbs layout rounding.
inline constexpr int kFocusEdgeTolerance = 1;

[[nodiscard]] inline constexpr int RectRight(const Rect& rect) noexcept {
  return rect.x + rect.width;
}
[[nodiscard]] inline constexpr int RectBottom(const Rect& rect) noexcept {
  return rect.y + rect.height;
}

// Negative when `candidate` does not lie that way.
[[nodiscard]] inline constexpr int DirectionalGap(
    const Rect& from, const Rect& candidate, Direction direction) noexcept {
  switch (direction) {
    case Direction::kLeft:
      return from.x - RectRight(candidate);
    case Direction::kRight:
      return candidate.x - RectRight(from);
    case Direction::kUp:
      return from.y - RectBottom(candidate);
    default:  // kDown
      return candidate.y - RectBottom(from);
  }
}

// The panes' overlap on the cross axis; <= 0 when they do not face at all.
[[nodiscard]] inline constexpr int DirectionalOverlap(
    const Rect& from, const Rect& candidate, Direction direction) noexcept {
  const bool isHorizontal =
      direction == Direction::kLeft || direction == Direction::kRight;
  if (isHorizontal) {
    return std::min(RectBottom(from), RectBottom(candidate)) -
           std::max(from.y, candidate.y);
  }
  return std::min(RectRight(from), RectRight(candidate)) -
         std::max(from.x, candidate.x);
}

// Chosen the way a tiling window manager does: the nearest pane past that edge
// that faces it, the one facing it most when several are as near.
[[nodiscard]] inline int DirectionalNeighbour(std::span<const LeafRect> leaves,
                                              int fromLeaf,
                                              Direction direction) {
  const Rect* from = nullptr;
  for (const LeafRect& leafRect : leaves) {
    if (leafRect.leaf == fromLeaf) {
      from = &leafRect.rect;
      break;
    }
  }
  if (!from) {
    return kNoLeaf;
  }
  int best = kNoLeaf;
  int bestDistance = 0;
  int bestFacing = 0;
  for (const LeafRect& leafRect : leaves) {
    if (leafRect.leaf == fromLeaf) {
      continue;
    }
    const int distance = DirectionalGap(*from, leafRect.rect, direction);
    const int facing = DirectionalOverlap(*from, leafRect.rect, direction);
    const bool isOnThatSide = distance >= -kFocusEdgeTolerance && facing > 0;
    if (!isOnThatSide) {
      continue;
    }
    const bool isNearer = distance < bestDistance - kFocusEdgeTolerance;
    const bool isAsNearButFacingMore =
        std::abs(distance - bestDistance) <= kFocusEdgeTolerance &&
        facing > bestFacing;
    const bool isBetter = best == kNoLeaf || isNearer || isAsNearButFacingMore;
    if (isBetter) {
      best = leafRect.leaf;
      bestDistance = distance;
      bestFacing = facing;
    }
  }
  return best;
}

// After every edit the tree is canonical: every split has >= 2 children, no split
// shares its parent's axis, and each split's weights are > 0 and sum to its child
// count (so 1.0 means "an average share"); see docs/re/drag-dock.md.

// Relative to the largest weight: float sums drift a little from exact thirds
// and quarters.
inline constexpr float kEqualWeightTolerance = 1e-3f;

inline void FixWeights(Node& node) {
  node.weights.resize(node.children.size(), 1.f);
  float sum = 0.f;
  for (float& weight : node.weights) {
    const bool isValidWeight = weight > 0.f && std::isfinite(weight);
    if (!isValidWeight) {
      weight = 1.f;
    }
    sum += weight;
  }
  const float scale = static_cast<float>(node.children.size()) / sum;
  for (float& weight : node.weights) {
    weight *= scale;
  }
}
[[nodiscard]] inline bool AllEqual(std::span<const float> weights) {
  float lowest = weights[0];
  float highest = weights[0];
  for (const float weight : weights) {
    lowest = std::min(lowest, weight);
    highest = std::max(highest, weight);
  }
  return highest - lowest <= kEqualWeightTolerance * highest;
}
[[nodiscard]] inline float Mean(std::span<const float> weights) {
  float sum = 0.f;
  for (const float weight : weights) {
    sum += weight;
  }
  return sum / static_cast<float>(weights.size());
}

// Bottom-up and idempotent: drop empty splits, replace a 1-child split by its
// child (which inherits the slot), and splice a same-axis child split into its
// parent with weights scaled by the child's slot, so on-screen sizes are kept.
inline void Normalize(Node& node) {
  if (node.isLeaf) {
    return;
  }
  FixWeights(node);
  std::vector<Node> keptChildren;
  std::vector<float> keptWeights;
  for (size_t i = 0; i < node.children.size(); i++) {
    Node& child = node.children[i];
    const float slotWeight = node.weights[i];
    Normalize(child);
    const bool isEmptySplit = !child.isLeaf && child.children.empty();
    if (isEmptySplit) {
      continue;
    }
    const bool isSameAxisSplit = !child.isLeaf && child.axis == node.axis;
    if (isSameAxisSplit) {
      float childWeightSum = 0.f;
      for (const float weight : child.weights) {
        childWeightSum += weight;
      }
      for (size_t j = 0; j < child.children.size(); j++) {
        keptChildren.push_back(std::move(child.children[j]));
        keptWeights.push_back(slotWeight * child.weights[j] / childWeightSum);
      }
    } else {
      keptChildren.push_back(std::move(child));
      keptWeights.push_back(slotWeight);
    }
  }
  node.children = std::move(keptChildren);
  node.weights = std::move(keptWeights);
  const bool hasSingleChild = node.children.size() == 1;
  if (hasSingleChild) {
    // Moved out first: the child lives inside `node`, which the assignment
    // overwrites.
    Node onlyChild = std::move(node.children[0]);
    node = std::move(onlyChild);
    return;
  }
  if (!node.children.empty()) {
    FixWeights(node);
  }
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

// A square big enough that every pane of a deep tree keeps a distinct rect.
inline constexpr int kReadingOrderExtent = 10000;

// The leaves as the panes read on screen: top to bottom, then left to right by
// their top-left corners. Not the tree's own order, which reads a grid built as
// columns down each column.
[[nodiscard]] inline std::vector<int> ReadingOrder(const Node& tree) {
  std::vector<LeafRect> leaves;
  LayoutTree(tree, Rect{0, 0, kReadingOrderExtent, kReadingOrderExtent}, 0,
             leaves);
  std::stable_sort(leaves.begin(), leaves.end(),
                   [](const LeafRect& first, const LeafRect& second) {
                     if (first.rect.y != second.rect.y) {
                       return first.rect.y < second.rect.y;
                     }
                     return first.rect.x < second.rect.x;
                   });
  std::vector<int> order;
  order.reserve(leaves.size());
  for (const LeafRect& leafRect : leaves) {
    order.push_back(leafRect.leaf);
  }
  return order;
}

// The strip between child `index` and `index + 1` of the split at `path`
// (child indices from the root). For a horizontal split it is a column from
// x = `position`; for a vertical split, a row from y = `position`.
// `lengthBefore` / `lengthAfter` are the two children's lengths when measured.
struct Seam {
  std::vector<int> path;
  int index = 0;
  Axis axis = Axis::kHorizontal;
  int position = 0;
  int thickness = 0;
  int crossStart = 0;
  int crossLength = 0;
  int lengthBefore = 0;
  int lengthAfter = 0;
};

inline void CollectSeams(const Node& node, const Rect& bounds, int gap,
                         std::vector<int>& path, std::vector<Seam>& out) {
  if (node.isLeaf) {
    return;
  }
  const std::vector<Rect> rects = ChildRects(node, bounds, gap);
  const bool isHorizontal = node.axis == Axis::kHorizontal;
  for (size_t i = 0; i + 1 < rects.size(); i++) {
    Seam seam;
    seam.path = path;
    seam.index = static_cast<int>(i);
    seam.axis = node.axis;
    seam.position = isHorizontal ? rects[i].x + rects[i].width
                                 : rects[i].y + rects[i].height;
    seam.thickness = gap;
    seam.crossStart = isHorizontal ? bounds.y : bounds.x;
    seam.crossLength = isHorizontal ? bounds.height : bounds.width;
    seam.lengthBefore = isHorizontal ? rects[i].width : rects[i].height;
    seam.lengthAfter = isHorizontal ? rects[i + 1].width : rects[i + 1].height;
    out.push_back(std::move(seam));
  }
  for (size_t i = 0; i < rects.size(); i++) {
    path.push_back(static_cast<int>(i));
    CollectSeams(node.children[i], rects[i], gap, path, out);
    path.pop_back();
  }
}

[[nodiscard]] inline std::vector<Seam> Seams(const Node& root,
                                             const Rect& bounds, int gap) {
  std::vector<Seam> seams;
  std::vector<int> path;
  CollectSeams(root, bounds, gap, path, seams);
  return seams;
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

// Only the two children the seam divides trade length; their combined weight is
// unchanged, so every other proportion holds. `seam` is the measurement taken
// when the drag began, so the result depends only on the total delta.
inline bool ResizeSeam(Node& root, const Seam& seam, int delta) {
  Node* split = NodeAt(root, seam.path);
  const bool isSeamOfSplit =
      split && !split->isLeaf &&
      static_cast<size_t>(seam.index) + 1 < split->children.size();
  if (!isSeamOfSplit) {
    return false;
  }
  FixWeights(*split);
  const int combinedLength = seam.lengthBefore + seam.lengthAfter;
  if (combinedLength <= 0) {
    return false;
  }
  const int minLength = std::min(kDesiredMinPane, combinedLength / 2);
  const int lengthBefore = std::clamp(seam.lengthBefore + delta, minLength,
                                      combinedLength - minLength);
  float& weightBefore = split->weights[static_cast<size_t>(seam.index)];
  float& weightAfter = split->weights[static_cast<size_t>(seam.index) + 1];
  const float combinedWeight = weightBefore + weightAfter;
  weightBefore = combinedWeight * static_cast<float>(lengthBefore) /
                 static_cast<float>(combinedLength);
  weightAfter = combinedWeight - weightBefore;
  return true;
}

inline bool EqualizeSplit(Node& root, const std::vector<int>& path) {
  Node* split = NodeAt(root, path);
  const bool isUnevenSplit = split && !split->isLeaf &&
                             !split->weights.empty() &&
                             !AllEqual(split->weights);
  if (!isUnevenSplit) {
    return false;
  }
  std::fill(split->weights.begin(), split->weights.end(), 1.f);
  return true;
}

}  // namespace fixedge
