// pane_neighbours.h — the pane on a given side of another, by their laid-out
// rects, for moving the focus between panes.
#pragma once
#include <algorithm>
#include <cmath>
#include <span>
#include "edge/geom.h"
#include "layout/pane_tree.h"

namespace edgeplus {

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

}  // namespace edgeplus
