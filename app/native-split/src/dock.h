// dock.h — where a dragged pane would land. One concern, zero Edge/Win32 deps,
// so it is unit-testable: map a pointer position over the split to a drop zone,
// apply that edit to a copy of the tree, and report the resulting tree plus the
// rect the dragged pane will occupy in it (the preview is the real outcome, not
// an approximation). The dragged pane is one of the split's own (its grip) or
// an incoming one (a tab dragged in from the strip). The zone rules follow
// dockview / FlexLayout / react-mosaic (see docs/re/drag-dock.md); all lengths
// are DIPs in the split's coordinates.
#pragma once
#include <algorithm>
#include <vector>
#include "geom.h"
#include "pane_tree.h"

namespace fixedge::dock {

enum class Zone : uint8_t {
  kNone,      // nowhere to drop (outside, over the dragged pane, or a no-op)
  kPaneEdge,  // beside a pane: `target` + `direction`
  kSwap,      // a pane's centre: trade places with `target`
  kRootEdge,  // an outer edge of the whole split: span it (`direction`)
};

// How far in from an outer edge the pointer targets the whole split: a share
// of the split's extent along that axis, so dragging "far enough" toward an
// edge spans it - generous on a big split, never thinner than a fingertip.
inline constexpr float kRootBandShare = 0.08f;
inline constexpr int kRootBandMin = 24;
inline constexpr int kRootBandMax = 72;
// An active outer band holds this much farther in (hysteresis).
inline constexpr int kRootReleaseExtra = 32;
// A pointer this far outside the split still counts (it is clamped in).
inline constexpr int kOvershoot = 48;
// A pane's centre box spans [0.25, 0.75) on both axes.
inline constexpr float kCentre = 0.25f;
// An active pane edge holds until 40% into the pane.
inline constexpr float kEdgeHold = 0.4f;
// A pointer pixel is sampled at its centre, so a pane's first and last pixels
// sit symmetrically inside 0..1.
inline constexpr float kPixelCentre = 0.5f;

// Where the dragged pane comes from.
enum class Source : uint8_t {
  kMoving,    // one of the split's panes: the drop moves it
  kIncoming,  // a new pane (a tab from the strip): the drop inserts it, and
              // only the split itself counts (it arrives from above, so an
              // overshoot would be the strip, not the split's top edge)
};

struct Plan {
  Zone zone = Zone::kNone;
  int target = kNoLeaf;  // the pane dropped on (kPaneEdge / kSwap)
  Direction direction = Direction::kUp;
  Node result;   // the tree after the drop
  Rect preview;  // the dragged pane's rect in `result`
};

inline int DistanceToEdge(const Rect& rect, const Point& point,
                          Direction edge) {
  switch (edge) {
    case Direction::kLeft:
      return point.x - rect.x;
    case Direction::kRight:
      return rect.x + rect.width - 1 - point.x;
    case Direction::kUp:
      return point.y - rect.y;
    default:
      return rect.y + rect.height - 1 - point.y;
  }
}

// The outer band's depth for edge `edge` of `layout`.
inline int RootBand(const Rect& layout, Direction edge) {
  const bool isVerticalEdge =
      edge == Direction::kLeft || edge == Direction::kRight;
  const int extent = isVerticalEdge ? layout.width : layout.height;
  const int share =
      static_cast<int>(static_cast<float>(extent) * kRootBandShare);
  return std::clamp(share, kRootBandMin, kRootBandMax);
}

// Whether a pane-relative position (`paneX`, `paneY` in 0..1) is still within
// the hold band of edge `edge`, so a zone already showing does not flip at the
// diagonal.
inline bool HoldsEdge(Direction edge, float paneX, float paneY) {
  switch (edge) {
    case Direction::kLeft:
      return paneX < kEdgeHold;
    case Direction::kRight:
      return paneX >= 1.f - kEdgeHold;
    case Direction::kUp:
      return paneY < kEdgeHold;
    default:
      return paneY >= 1.f - kEdgeHold;
  }
}

// The nearest edge by the pane's diagonals (FlexLayout): no dead corners.
inline Direction DiagonalEdge(float paneX, float paneY) {
  const bool isBelowMainDiagonal = paneY >= paneX;        // the "\" diagonal
  const bool isBelowAntiDiagonal = paneY >= 1.f - paneX;  // the "/" diagonal
  if (isBelowMainDiagonal) {
    return isBelowAntiDiagonal ? Direction::kDown : Direction::kLeft;
  }
  return isBelowAntiDiagonal ? Direction::kRight : Direction::kUp;
}

// Drop `pane` on `zone` of `tree`: move it there, or insert it if incoming.
// Returns false if nothing would change (an incoming pane cannot swap).
inline bool ApplyDrop(Node& tree, int pane, Source source, Zone zone,
                      int target, Direction direction) {
  const bool isMoving = source == Source::kMoving;
  switch (zone) {
    case Zone::kPaneEdge:
      if (isMoving) {
        return MovePane(tree, pane, target, direction);
      }
      return SplitPane(tree, target, direction, pane);
    case Zone::kSwap:
      return isMoving && SwapLeaves(tree, pane, target);
    case Zone::kRootEdge:
      if (isMoving) {
        return MovePaneToRootEdge(tree, pane, direction);
      }
      InsertAtRootEdge(tree, pane, direction);
      return true;
    default:
      return false;
  }
}

// Finish a plan: apply the edit to a copy; a no-op yields kNone.
inline Plan Finish(const Node& tree, const Rect& layout, int pane,
                   Source source, Zone zone, int target, Direction direction) {
  Plan plan;
  plan.result = tree;
  const bool isChanged =
      ApplyDrop(plan.result, pane, source, zone, target, direction);
  if (!isChanged) {
    return {};
  }
  std::vector<LeafRect> leaves;
  LayoutTree(plan.result, layout, kPaneGap, leaves);
  for (const LeafRect& leafRect : leaves) {
    const bool isDraggedPane = leafRect.leaf == pane;
    if (isDraggedPane) {
      plan.preview = leafRect.rect;
    }
  }
  plan.zone = zone;
  plan.target = target;
  plan.direction = direction;
  return plan;
}

// Resolve `point` (split coordinates) while dragging `pane` from `source`.
// `shown` is the plan currently shown, for hysteresis. Precedence: the whole
// split's outer band first (checked before any pane, as dockview's root target
// is), then the pane under the pointer — its centre swaps (a moving pane only),
// its edges dock beside it; the gap between two panes keeps what is shown. So
// near a pane's bottom edge docks below that pane, and dragging on, deep
// toward the split's bottom, spans the whole bottom.
inline Plan ResolveDrop(const Node& tree, const Rect& layout, Point point,
                        int pane, Source source, const Plan& shown) {
  if (Empty(layout)) {
    return {};
  }
  const bool isMoving = source == Source::kMoving;
  const int overshoot = isMoving ? kOvershoot : 0;
  const Rect reach{layout.x - overshoot, layout.y - overshoot,
                   layout.width + 2 * overshoot, layout.height + 2 * overshoot};
  if (!Contains(reach, point)) {
    return {};
  }
  point.x = std::clamp(point.x, layout.x, layout.x + layout.width - 1);
  point.y = std::clamp(point.y, layout.y, layout.y + layout.height - 1);

  // 1. The whole split's outer band.
  bool hasRootEdge = false;
  Direction rootEdge = Direction::kUp;
  const bool isHoldingRootEdge =
      shown.zone == Zone::kRootEdge &&
      DistanceToEdge(layout, point, shown.direction) <=
          RootBand(layout, shown.direction) + kRootReleaseExtra;
  if (isHoldingRootEdge) {
    hasRootEdge = true;
    rootEdge = shown.direction;
  } else {
    int nearestDistance = 0;
    for (const Direction edge : {Direction::kLeft, Direction::kRight,
                                 Direction::kUp, Direction::kDown}) {
      const int distance = DistanceToEdge(layout, point, edge);
      const bool isNearerEdgeInBand =
          distance < RootBand(layout, edge) &&
          (!hasRootEdge || distance < nearestDistance);
      if (isNearerEdgeInBand) {
        nearestDistance = distance;
        rootEdge = edge;
        hasRootEdge = true;
      }
    }
  }
  if (hasRootEdge) {
    Plan plan =
        Finish(tree, layout, pane, source, Zone::kRootEdge, kNoLeaf, rootEdge);
    const bool isRootEdgeMove = plan.zone != Zone::kNone;
    if (isRootEdgeMove) {
      return plan;  // a no-op falls through to the pane
    }
  }

  // 2. The pane under the pointer (the dragged pane itself offers nothing).
  std::vector<LeafRect> leaves;
  LayoutTree(tree, layout, kPaneGap, leaves);
  for (const LeafRect& leafRect : leaves) {
    const Rect& paneRect = leafRect.rect;
    if (!Contains(paneRect, point)) {
      continue;
    }
    const bool canDropOnPane = leafRect.leaf != pane && !Empty(paneRect);
    if (!canDropOnPane) {
      return {};
    }
    const float paneX =
        (static_cast<float>(point.x - paneRect.x) + kPixelCentre) /
        static_cast<float>(paneRect.width);
    const float paneY =
        (static_cast<float>(point.y - paneRect.y) + kPixelCentre) /
        static_cast<float>(paneRect.height);
    const bool isHoldingPaneEdge = shown.zone == Zone::kPaneEdge &&
                                   shown.target == leafRect.leaf &&
                                   HoldsEdge(shown.direction, paneX, paneY);
    if (isHoldingPaneEdge) {
      return Finish(tree, layout, pane, source, Zone::kPaneEdge, leafRect.leaf,
                    shown.direction);
    }
    const bool isInCentre = paneX >= kCentre && paneX < 1.f - kCentre &&
                            paneY >= kCentre && paneY < 1.f - kCentre;
    const bool isSwap = isInCentre && isMoving;
    if (isSwap) {
      return Finish(tree, layout, pane, source, Zone::kSwap, leafRect.leaf,
                    Direction::kUp);
    }
    return Finish(tree, layout, pane, source, Zone::kPaneEdge, leafRect.leaf,
                  DiagonalEdge(paneX, paneY));
  }
  // 3. The gap between two panes belongs to neither: a pane's zone already
  // shown holds there, so crossing from pane to pane does not blink it off.
  const bool isPaneZoneShown =
      shown.zone == Zone::kPaneEdge || shown.zone == Zone::kSwap;
  if (isPaneZoneShown) {
    return shown;
  }
  return {};
}

// Dragging the split's own pane `moving` (its grip).
inline Plan Resolve(const Node& tree, const Rect& layout, Point point,
                    int moving, const Plan& shown) {
  return ResolveDrop(tree, layout, point, moving, Source::kMoving, shown);
}

// Dragging a new pane `incoming` (a tab) in: it joins the tree as that leaf.
inline Plan ResolveIncoming(const Node& tree, const Rect& layout, Point point,
                            int incoming, const Plan& shown) {
  return ResolveDrop(tree, layout, point, incoming, Source::kIncoming, shown);
}

}  // namespace fixedge::dock
