// pane_tree.h — the pure split-layout model. One concern, zero Edge/Win32 deps,
// so it is unit-testable in isolation (mirrors pade's pure reorder.ts). The tree
// is the single source of truth for where each pane sits; the geometry detour
// only translates its output into Edge ChildLayouts.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "geom.h"

namespace fixedge {

// A node splits its area among its children along one axis. Horizontal = children
// laid left->right; Vertical = stacked top->bottom. A leaf owns one container
// (by index into the MCV container vector).
enum class Axis : uint8_t {
  kHorizontal,
  kVertical
};

// A leaf id no container has: an unset leaf, a drop with no target pane.
inline constexpr int kNoLeaf = -1;

struct Node {
  bool isLeaf = true;
  int leaf = kNoLeaf;             // container index, when a leaf
  Axis axis = Axis::kHorizontal;  // split direction, when internal
  std::vector<Node> children;     // internal children, in order
  std::vector<float> weights;     // parallel to children; relative sizes
};

struct LeafRect {
  int leaf = kNoLeaf;
  Rect rect;
};

// The per-pane minimum length along the split axis (Edge floors panes similarly,
// so a resize can't crush one to nothing). Capped at an equal share of the span
// so a small window shrinks panes evenly instead of overflowing.
inline constexpr int kDesiredMinPane = 64;

// The strip left between neighbouring panes, where their divider sits: the
// room Edge's own split leaves its resize handle. No pane may cover a divider,
// since a page's native window takes every click over it.
inline constexpr int kPaneGap = 8;

// How a split's span is shared out among its panes.
struct SpanShare {
  std::vector<bool> isPinned;  // parallel to the weights
  float freeWeight = 0.f;      // the unpinned panes' total weight
  int freeSpan = 0;            // the length the unpinned panes share
};

// The length an unpinned pane of `weight` gets: its ratio of the free span.
[[nodiscard]] inline float FreeLength(const SpanShare& share, float weight) {
  return static_cast<float>(share.freeSpan) * (weight / share.freeWeight);
}

// Water-fill: pin to `minLength` only the panes whose proportional length
// would fall below it; the rest keep their exact ratio of what the pinned ones
// leave. Each round can push another pane under, so it repeats until none is.
[[nodiscard]] inline SpanShare WaterFill(const std::vector<float>& weights,
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

// Recursively place every leaf inside `bounds`, leaving `gap` px between siblings.
// Pure; early-returns on degenerate input. Each pane keeps at least a minimum
// length, and the extra is shared by weight with the last child absorbing
// rounding, so the panes exactly tile the bounds with no seam drift. Because the
// lengths come from `bounds`, the same tree re-laid in a resized window reflows
// proportionally — resize handling is intrinsic, not a special case.
// The rect of each child of split `node` inside `bounds`, `gap` px apart.
[[nodiscard]] inline std::vector<Rect> ChildRects(const Node& node,
                                                  const Rect& bounds, int gap) {
  std::vector<Rect> rects;
  const int childCount = static_cast<int>(node.children.size());
  if (node.isLeaf || childCount == 0) {
    return rects;
  }
  rects.reserve(node.children.size());

  // A child with no stored weight gets an average share.
  std::vector<float> weights(childCount);
  for (int i = 0; i < childCount; i++) {
    const bool hasWeight = static_cast<size_t>(i) < node.weights.size();
    weights[i] = hasWeight ? node.weights[i] : 1.f;
  }

  const bool isHorizontal = node.axis == Axis::kHorizontal;
  const int span = std::max(0, (isHorizontal ? bounds.width : bounds.height) -
                                   gap * (childCount - 1));
  // Feasible floor: never larger than an equal share, so the pins always fit.
  const int minLength = std::min(kDesiredMinPane, span / childCount);
  const SpanShare share = WaterFill(weights, span, minLength);
  const bool hasFreeWeight = share.freeWeight > 0.f;

  int cursor = isHorizontal ? bounds.x : bounds.y;
  int used = 0;
  for (int i = 0; i < childCount; i++) {
    int length = 0;
    const bool isLastChild = i == childCount - 1;
    if (isLastChild) {
      length = span - used;  // the last absorbs rounding so panes tile exactly
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

// A flat equal split of `count` leaves along `axis` — the proven behaviour and
// the fallback until the UI reshapes the tree.
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

// --- pure tree edits (the Arc behaviour: directional add, swap) ---------------

// The four directions a pane can be grown in. Up/Down split along the vertical
// axis (stacked); Left/Right along the horizontal. Up/Left put the new pane
// before the target; Down/Right after it.
enum class Direction : uint8_t {
  kUp,
  kDown,
  kLeft,
  kRight
};

[[nodiscard]] inline Axis AxisOf(Direction direction) {
  const bool isVertical =
      direction == Direction::kUp || direction == Direction::kDown;
  return isVertical ? Axis::kVertical : Axis::kHorizontal;
}
[[nodiscard]] inline bool IsBefore(Direction direction) {
  return direction == Direction::kUp || direction == Direction::kLeft;
}

// --- directional neighbour (pane focus) --------------------------------------
// The pane on a given side of another, chosen the way a tiling window manager
// does, matching the extension's Alt+Shift+arrow pane focus
// (extension/src/features/pane-focus/neighbour.ts): the nearest pane past that
// edge whose shared side faces it, the one facing it most when several are as
// near. Operates on laid-out leaf rects, so it reads the same geometry the user
// sees. Panes sit kPaneGap apart, so a direct neighbour's gap is positive; this
// tolerance only absorbs layout rounding.
inline constexpr int kFocusEdgeTolerance = 1;

[[nodiscard]] inline int RectRight(const Rect& rect) {
  return rect.x + rect.width;
}
[[nodiscard]] inline int RectBottom(const Rect& rect) {
  return rect.y + rect.height;
}

// How far `candidate` lies past `from` in `direction`; negative when it does
// not lie that way.
[[nodiscard]] inline int DirectionalGap(const Rect& from, const Rect& candidate,
                                        Direction direction) {
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

// How much of the two panes' shared side faces each other (their overlap on the
// cross axis); <= 0 when they do not face at all.
[[nodiscard]] inline int DirectionalOverlap(const Rect& from,
                                            const Rect& candidate,
                                            Direction direction) {
  const bool isHorizontal =
      direction == Direction::kLeft || direction == Direction::kRight;
  if (isHorizontal) {
    return std::min(RectBottom(from), RectBottom(candidate)) -
           std::max(from.y, candidate.y);
  }
  return std::min(RectRight(from), RectRight(candidate)) -
         std::max(from.x, candidate.x);
}

// The leaf on `direction`'s side of `fromLeaf` among `leaves`, or kNoLeaf if
// there is none.
[[nodiscard]] inline int DirectionalNeighbour(
    const std::vector<LeafRect>& leaves, int fromLeaf, Direction direction) {
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

// --- canonical form + weights -------------------------------------------------
// After every edit the tree is canonical: every split has >= 2 children, no split
// shares its parent's axis, and each split's weights are > 0 and sum to its child
// count (so 1.0 means "an average share"). The approach is the one shared by
// VS Code's grid, dockview and react-mosaic (see docs/re/drag-dock.md).

// How far apart, relative to the largest, weights may be and still count as
// equal — float sums drift a little from exact thirds and quarters.
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
[[nodiscard]] inline bool AllEqual(const std::vector<float>& weights) {
  float lowest = weights[0];
  float highest = weights[0];
  for (const float weight : weights) {
    lowest = std::min(lowest, weight);
    highest = std::max(highest, weight);
  }
  return highest - lowest <= kEqualWeightTolerance * highest;
}
[[nodiscard]] inline float Mean(const std::vector<float>& weights) {
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

// --- lookup -------------------------------------------------------------------

[[nodiscard]] inline bool IsLeafFor(const Node& node, int leaf) {
  return node.isLeaf && node.leaf == leaf;
}

// The split directly holding `leaf`, and its index there. Leaves are found by
// id, never by a stored path, so a lookup after an edit is always fresh.
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

// Replace `node` by a 50/50 split along `axis` of itself and the pane `leaf`,
// the new pane first when `isLeafFirst`. The split keeps `node`'s slot.
inline void WrapWithLeaf(Node& node, Axis axis, bool isLeafFirst, int leaf) {
  Node old = std::move(node);
  node = isLeafFirst ? MakeSplit(axis, MakeLeaf(leaf), std::move(old))
                     : MakeSplit(axis, std::move(old), MakeLeaf(leaf));
}

// --- edits ----------------------------------------------------------------------

// Place `leaf` next to `target` on side `direction`. If the target's split
// already runs along that axis the leaf joins it: an equal row stays equal, a
// resized row only gives up half of the target. Otherwise the target is wrapped
// in a new 50/50 split that keeps the target's slot. Returns false if the
// target is missing.
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

// Add a NEW pane `newLeaf` next to `targetLeaf` on side `direction`
// (directional add).
inline bool SplitPane(Node& root, int targetLeaf, Direction direction,
                      int newLeaf) {
  const bool isInserted = InsertBeside(root, targetLeaf, direction, newLeaf);
  if (!isInserted) {
    return false;
  }
  Normalize(root);
  return true;
}

// Remove `leaf` from the tree; its weight is dropped so the siblings grow in
// proportion. Refuses to remove the last pane. Returns false if not found.
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

// Every leaf id above `removed` moves down by one: the containers after a closed
// pane's are re-fed one slot earlier, and the tree follows its tabs.
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

// Closing pane `leaf`: its siblings share its space in proportion (a sibling
// in another split is untouched), and later leaf ids close the gap. Returns
// false if the leaf is missing or is the last pane.
inline bool RemovePane(Node& root, int leaf) {
  if (!Detach(root, leaf)) {
    return false;
  }
  RenumberAfterRemoval(root, leaf);
  return true;
}

// Move child `from` of `parent` to insert-before position `to` (a pre-removal
// index), carrying its weight.
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

// Drop onto a pane's edge: move `moving` next to `target` on side `direction`.
// Within the same split along that axis it is a pure reorder (the pane keeps
// its size); otherwise remove it, then insert beside the target found afresh by
// id. Returns false for a no-op (itself, missing ids, or already there).
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

// Drop onto an outer edge of the whole layout: `moving` spans that edge. A root
// along the other axis is wrapped 50/50 (the pane takes the full half); a root
// along the same axis gains it as an end child with an average share.
// Returns false for a no-op (single pane, missing, or already spanning that edge).
// Add `leaf` (not yet in the tree) along the whole `direction` edge of the
// split: one more child of a root split along that axis, at the average share,
// else a new root of the old root and `leaf`.
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

// Drop onto a pane's centre: the two panes trade places; the sizes stay with the
// positions. Returns false if either is missing or they are the same pane.
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

// Sets `out` to the axis of the split directly holding `leaf`; false if no
// split holds it.
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
// The axis of the split a leaf belongs to — i.e. which way its swap toggle faces
// (vertical parent → top/bottom swap, horizontal → left/right). Returns
// kHorizontal if not found.
[[nodiscard]] inline Axis SwapAxisOf(const Node& root, int leaf) {
  Axis axis = Axis::kHorizontal;
  SwapAxisIn(root, leaf, axis);
  return axis;
}

// Swap a leaf with its adjacent sibling along the parent's axis (its "counterpart"
// in the spec). Returns true if a swap happened. Pure — operates on the tree's
// leaf indices, so the caller does not move any Edge object.
inline bool SwapWithCounterpart(Node& node, int leaf) {
  for (size_t i = 0; i < node.children.size(); i++) {
    if (IsLeafFor(node.children[i], leaf)) {
      // Prefer the next sibling, else the previous — the counterpart on the axis.
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

// Collect leaf indices left-to-right / top-to-bottom. Pure.
inline void CollectLeaves(const Node& node, std::vector<int>& out) {
  if (node.isLeaf) {
    out.push_back(node.leaf);
    return;
  }
  for (const Node& child : node.children) {
    CollectLeaves(child, out);
  }
}

// How many panes `node` tiles. Pure.
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

// --- seams: where neighbouring panes meet, and dragging them -------------------

// The strip between child `index` and `index + 1` of the split at `path`
// (child indices from the root), as laid out `thickness` apart. For a
// horizontal split it is a column from x = `position`, `crossStart`..
// `crossStart + crossLength` tall; for a vertical split, a row from y =
// `position`. `lengthBefore` / `lengthAfter` are the two children's lengths
// along the split when measured.
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

// Every seam of `root` laid out in `bounds` with `gap` px between its panes
// (LayoutTree's gap): a tree of N panes has N - 1.
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

// Drag `seam` by `delta` px along its split: the two children it divides trade
// that much length (each kept at least kDesiredMinPane), and nothing else
// moves. Their combined weight is unchanged, so every other proportion holds.
// `seam` is the measurement taken when the drag began, so the result depends
// only on the total delta. Returns false if the seam no longer exists.
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

// Double-click on a divider: every child of the split at `path` gets an equal
// share again. Returns false if there is no split there or it is already even.
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
