// dock.h — where a dragged pane would land. One concern, zero Edge/Win32 deps,
// so it is unit-testable: map a pointer position over the split to a drop zone,
// apply that edit to a copy of the tree, and report the resulting tree plus the
// rect the dragged pane will occupy in it (the preview is the real outcome, not
// an approximation). The zone rules follow dockview / FlexLayout / react-mosaic
// (see docs/re/drag-dock.md); all lengths are DIPs in the split's coordinates.
#pragma once
#include <algorithm>
#include <vector>
#include "geom.h"
#include "pane_tree.h"

namespace fixedge::dock {

enum class Zone : uint8_t {
  kNone,      // nowhere to drop (outside, over the dragged pane, or a no-op)
  kPaneEdge,  // beside a pane: `target` + `dir`
  kSwap,      // a pane's centre: trade places with `target`
  kRootEdge,  // an outer edge of the whole split: span it (`dir`)
};

// How far in from an outer edge the pointer targets the whole split: a share
// of the split's extent along that axis, so dragging "far enough" toward an
// edge spans it - generous on a big split, never thinner than a fingertip.
inline constexpr float kRootBandShare = 0.08f;
inline constexpr int kRootBandMin = 24;
inline constexpr int kRootBandMax = 72;
// An active outer band holds this much farther in (hysteresis).
inline constexpr int kRootReleaseExtra = 32;
inline constexpr int kOvershoot =
    48;  // a pointer this far outside still counts (clamped in)
inline constexpr float kCentre =
    0.25f;  // a pane's centre box spans [0.25, 0.75) on both axes
inline constexpr float kEdgeHold =
    0.4f;  // an active pane edge holds until 40% into the pane

struct Plan {
  Zone zone = Zone::kNone;
  int target = -1;  // the pane dropped on (kPaneEdge / kSwap)
  Direction dir = Direction::kUp;
  Node result;   // the tree after the drop
  Rect preview;  // the dragged pane's rect in `result`
};

inline int DistanceToEdge(const Rect& r, const Point& p, Direction d) {
  switch (d) {
    case Direction::kLeft:
      return p.x - r.x;
    case Direction::kRight:
      return r.x + r.w - 1 - p.x;
    case Direction::kUp:
      return p.y - r.y;
    default:
      return r.y + r.h - 1 - p.y;
  }
}

// The outer band's depth for edge `edge` of `layout`.
inline int RootBand(const Rect& layout, Direction edge) {
  const bool isVerticalEdge =
      edge == Direction::kLeft || edge == Direction::kRight;
  const int extent = isVerticalEdge ? layout.w : layout.h;
  const int share = static_cast<int>(static_cast<float>(extent) * kRootBandShare);
  return std::clamp(share, kRootBandMin, kRootBandMax);
}

// Whether a pane-relative position (u, v in 0..1) is still within the hold band
// of edge `d`, so a zone already showing does not flip at the diagonal.
inline bool HoldsEdge(Direction d, float u, float v) {
  switch (d) {
    case Direction::kLeft:
      return u < kEdgeHold;
    case Direction::kRight:
      return u >= 1.f - kEdgeHold;
    case Direction::kUp:
      return v < kEdgeHold;
    default:
      return v >= 1.f - kEdgeHold;
  }
}

// The nearest edge by the pane's diagonals (FlexLayout): no dead corners.
inline Direction DiagonalEdge(float u, float v) {
  const bool below_main = v >= u;        // bottom-left of the "\" diagonal
  const bool below_anti = v >= 1.f - u;  // bottom-right of the "/" diagonal
  if (below_main) {
    return below_anti ? Direction::kDown : Direction::kLeft;
  }
  return below_anti ? Direction::kRight : Direction::kUp;
}

// Finish a plan: apply the edit to a copy; a no-op yields kNone.
inline Plan Finish(const Node& tree, const Rect& layout, int moving, Zone zone,
                   int target, Direction dir) {
  Plan plan;
  plan.result = tree;
  bool changed = false;
  switch (zone) {
    case Zone::kPaneEdge:
      changed = MovePane(plan.result, moving, target, dir);
      break;
    case Zone::kSwap:
      changed = SwapLeaves(plan.result, moving, target);
      break;
    case Zone::kRootEdge:
      changed = MovePaneToRootEdge(plan.result, moving, dir);
      break;
    default:
      break;
  }
  if (!changed) {
    return {};
  }
  std::vector<LeafRect> leaves;
  LayoutTree(plan.result, layout, /*gap=*/0, leaves);
  for (const LeafRect& lr : leaves) {
    if (lr.leaf == moving) {
      plan.preview = lr.rect;
    }
  }
  plan.zone = zone;
  plan.target = target;
  plan.dir = dir;
  return plan;
}

// Resolve pointer `p` (split coordinates) while dragging pane `moving`. `prev` is
// the plan currently shown, for hysteresis. Precedence: the whole split's outer
// band first (checked before any pane, as dockview's root target is), then the
// pane under the pointer — its centre swaps, its edges dock beside it. So near
// a pane's bottom edge docks below that pane, and dragging on, deep toward the
// split's bottom, spans the whole bottom.
inline Plan Resolve(const Node& tree, const Rect& layout, Point p, int moving,
                    const Plan& prev) {
  if (layout.w <= 0 || layout.h <= 0) {
    return {};
  }
  if (p.x < layout.x - kOvershoot || p.x >= layout.x + layout.w + kOvershoot ||
      p.y < layout.y - kOvershoot || p.y >= layout.y + layout.h + kOvershoot) {
    return {};
  }
  p.x = std::clamp(p.x, layout.x, layout.x + layout.w - 1);
  p.y = std::clamp(p.y, layout.y, layout.y + layout.h - 1);

  // 1. The whole split's outer band.
  bool root = false;
  Direction root_dir = Direction::kUp;
  const bool isHoldingRootEdge =
      prev.zone == Zone::kRootEdge &&
      DistanceToEdge(layout, p, prev.dir) <=
          RootBand(layout, prev.dir) + kRootReleaseExtra;
  if (isHoldingRootEdge) {
    root = true;
    root_dir = prev.dir;
  } else {
    int nearest = 0;
    for (Direction d : {Direction::kLeft, Direction::kRight, Direction::kUp,
                        Direction::kDown}) {
      const int dist = DistanceToEdge(layout, p, d);
      const bool isNearerEdgeInBand =
          dist < RootBand(layout, d) && (!root || dist < nearest);
      if (isNearerEdgeInBand) {
        nearest = dist;
        root_dir = d;
        root = true;
      }
    }
  }
  if (root) {
    Plan plan = Finish(tree, layout, moving, Zone::kRootEdge, -1, root_dir);
    if (plan.zone != Zone::kNone) {
      return plan;  // a no-op falls through to the pane
    }
  }

  // 2. The pane under the pointer (the dragged pane itself offers nothing).
  std::vector<LeafRect> leaves;
  LayoutTree(tree, layout, /*gap=*/0, leaves);
  for (const LeafRect& lr : leaves) {
    if (!Contains(lr.rect, p)) {
      continue;
    }
    if (lr.leaf == moving || lr.rect.w <= 0 || lr.rect.h <= 0) {
      return {};
    }
    const float u = (static_cast<float>(p.x - lr.rect.x) + 0.5f) /
                    static_cast<float>(lr.rect.w);
    const float v = (static_cast<float>(p.y - lr.rect.y) + 0.5f) /
                    static_cast<float>(lr.rect.h);
    if (prev.zone == Zone::kPaneEdge && prev.target == lr.leaf &&
        HoldsEdge(prev.dir, u, v)) {
      return Finish(tree, layout, moving, Zone::kPaneEdge, lr.leaf, prev.dir);
    }
    if (u >= kCentre && u < 1.f - kCentre && v >= kCentre &&
        v < 1.f - kCentre) {
      return Finish(tree, layout, moving, Zone::kSwap, lr.leaf, Direction::kUp);
    }
    return Finish(tree, layout, moving, Zone::kPaneEdge, lr.leaf,
                  DiagonalEdge(u, v));
  }
  return {};
}

}  // namespace fixedge::dock
