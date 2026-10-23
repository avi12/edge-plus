// tab_edge.h — the geometry behind tab_shape.h's tab outline: a straight
// leading edge (the right one when Edge's UI runs right to left), the seam
// between members of a split, and the strip that squares a split group's fill.
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

// The trailing top corner keeps the radius (tab_shape.h turns it into a cut)
// unless another member of a split sits against it in the same row.
[[nodiscard]] constexpr CornerRadii RadiiFor(
    Side leading, float radius, bool hasTrailingNeighbor) noexcept {
  const CornerRadius round{radius, radius};
  const CornerRadius square{};
  const CornerRadius trailingTop = hasTrailingNeighbor ? square : round;
  const bool isLeadingOnLeft = leading == Side::kLeft;
  CornerRadii radii;
  radii.corners[static_cast<size_t>(Corner::kUpperLeft)] =
      isLeadingOnLeft ? square : trailingTop;
  radii.corners[static_cast<size_t>(Corner::kUpperRight)] =
      isLeadingOnLeft ? trailingTop : square;
  return radii;
}

// How far apart two members of a split may be and still be one seam.
inline constexpr int kSeamSlack = 2;

[[nodiscard]] constexpr bool HasTrailingNeighbor(Rect tab, Side leading,
                                                 const Rect* others,
                                                 size_t count) noexcept {
  const int seamX = leading == Side::kLeft ? tab.x + tab.width : tab.x;
  for (size_t i = 0; i < count; i++) {
    const Rect other = others[i];
    const bool isInRow = other.y == tab.y && other.height == tab.height;
    const int otherSeamX =
        leading == Side::kLeft ? other.x : other.x + other.width;
    const bool isTouching =
        otherSeamX >= seamX - kSeamSlack && otherSeamX <= seamX + kSeamSlack;
    const bool isOther = !(other == tab);
    if (isOther && isInRow && isTouching) {
      return true;
    }
  }
  return false;
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
