// tab_edge.h — the geometry behind tab_shape.h's tab outline: a straight
// leading edge (the right one when Edge's UI runs right to left), which tabs
// keep the trailing cut, and the strip that squares a split group's fill.
// Pure.
#pragma once
#include <cstddef>
#include "geom.h"

namespace fixedge::tab_edge {

// Where the favicon is.
enum class Side {
  kLeft,
  kRight,
};

[[nodiscard]] constexpr Side LeadingSide(bool isRightToLeft) noexcept {
  return isRightToLeft ? Side::kRight : Side::kLeft;
}

// SkVector.
using CornerRadius = PointF;

// In Skia's order.
enum class Corner {
  kUpperLeft,
  kUpperRight,
  kLowerRight,
  kLowerLeft,
  kCount,
};
inline constexpr size_t kCornerCount = static_cast<size_t>(Corner::kCount);

struct CornerRadii {
  CornerRadius corners[kCornerCount];
};

// The cut marks a tab of its own, and in a split the focused pane's tab only:
// the split's other members stay square, however many there are.
[[nodiscard]] constexpr bool IsTrailingCornerCut(bool isSplitMember,
                                                 bool isFocusedPane) noexcept {
  return !isSplitMember || isFocusedPane;
}

// The trailing bottom corner keeps the radius (tab_shape.h turns it into a cut)
// only when it is cut.
[[nodiscard]] constexpr CornerRadii RadiiFor(Side leading, float radius,
                                             bool isTrailingCut) noexcept {
  const CornerRadius round{radius, radius};
  const CornerRadius square{};
  const CornerRadius trailingBottom = isTrailingCut ? round : square;
  const bool isLeadingOnLeft = leading == Side::kLeft;
  CornerRadii radii;
  radii.corners[static_cast<size_t>(Corner::kLowerLeft)] =
      isLeadingOnLeft ? square : trailingBottom;
  radii.corners[static_cast<size_t>(Corner::kLowerRight)] =
      isLeadingOnLeft ? trailingBottom : square;
  return radii;
}

// Painted over a rounded rectangle, squares its leading corners.
[[nodiscard]] constexpr Rect LeadingStrip(Rect rect, Side leading,
                                          int width) noexcept {
  const int stripWidth = width < rect.width ? width : rect.width;
  const int stripX =
      leading == Side::kLeft ? rect.x : rect.x + rect.width - stripWidth;
  return Rect{stripX, rect.y, stripWidth, rect.height};
}

}  // namespace fixedge::tab_edge
