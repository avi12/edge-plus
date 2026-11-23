// pane_layout.h — the tree laid out in a rectangle: each split shares its span
// by weight with no pane below its minimum and kPaneGap between panes, and the
// panes in reading order.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>
#include "edge/geom.h"
#include "layout/pane_tree.h"

namespace edgeplus {

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

}  // namespace edgeplus
