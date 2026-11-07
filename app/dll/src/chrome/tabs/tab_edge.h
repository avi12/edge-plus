// tab_edge.h — the geometry behind tab_shape.h's tab outline: a straight
// leading edge (the right one when Edge's UI runs right to left), which tabs
// keep the trailing cut, and the strip that squares a split group's fill.
// Pure.
#pragma once
#include <cstddef>
#include "edge/geom.h"

namespace edgeplus::tab_edge {

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

// The bottom corner on the side a tab trails to.
[[nodiscard]] constexpr Corner TrailingBottomCorner(Side leading) noexcept {
  return leading == Side::kLeft ? Corner::kLowerRight : Corner::kLowerLeft;
}

inline constexpr size_t kCutOutlinePointCount = 6;

// A vertical tab's outline with its trailing bottom corner cut: a polygon
// through these points in order, the last one the first again. `leg` is how far
// the cut reaches along each side. Mirrored for a right-to-left UI by `leading`
// alone. (A horizontal tab is not mirrored here: Edge draws its outline left to
// right and flips the canvas under a right-to-left UI, so the cut that
// tab_shape.h puts on the outline's right shows on the left.)
struct CutOutline {
  PointF points[kCutOutlinePointCount];
};

[[nodiscard]] constexpr CutOutline CutOutlineOf(float left, float top,
                                                float right, float bottom,
                                                float leg,
                                                Side leading) noexcept {
  const bool isLeadingOnLeft = leading == Side::kLeft;
  const float leadingX = isLeadingOnLeft ? left : right;
  const float trailingX = isLeadingOnLeft ? right : left;
  const float cutX = isLeadingOnLeft ? trailingX - leg : trailingX + leg;
  return CutOutline{{{leadingX, top},
                     {trailingX, top},
                     {trailingX, bottom - leg},
                     {cutX, bottom},
                     {leadingX, bottom},
                     {leadingX, top}}};
}

// Painted over a rounded rectangle, squares its leading corners.
[[nodiscard]] constexpr Rect LeadingStrip(Rect rect, Side leading,
                                          int width) noexcept {
  const int stripWidth = width < rect.width ? width : rect.width;
  const int stripX =
      leading == Side::kLeft ? rect.x : rect.x + rect.width - stripWidth;
  return Rect{stripX, rect.y, stripWidth, rect.height};
}

}  // namespace edgeplus::tab_edge
