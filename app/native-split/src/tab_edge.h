// tab_edge.h — how a tab's leading edge is shaped. One concern: the pure
// geometry behind the tab outline (tab_shape.h): the corner radii that keep the
// leading edge (the left one, the right one when Edge's UI runs right to left)
// straight on every tab, the seam between members of a split, and the strip
// that squares a split group's fill. No Edge, no Win32.
#pragma once
#include <cstddef>
#include "geom.h"

namespace fixedge::tab_edge {

// The side of the tab that leads: where the favicon is.
enum class Side {
  kLeft,
  kRight,
};

[[nodiscard]] constexpr Side LeadingSide(bool isRightToLeft) noexcept {
  return isRightToLeft ? Side::kRight : Side::kLeft;
}

// One corner's radius, as Skia takes it (SkVector).
struct CornerRadius {
  float x = 0;
  float y = 0;

  [[nodiscard]] constexpr bool operator==(const CornerRadius&) const = default;
};

// The corners of a rounded rectangle, in Skia's order.
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

// A tab's corners with its leading side and its trailing bottom corner straight.
// Its trailing top corner keeps the radius, which tab_shape.h's outline turns
// into a cut, unless another member of a split sits against it in the same row:
// then the seam between the two is straight too.
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

// Whether a rect of `others` is `tab`'s neighbour on the trailing side: in its
// row and touching it there. Rects are in the same coordinates.
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

// The strip along `rect`'s leading side that squares a rounded rectangle's
// leading corners when painted over it: the full height, `width` across.
[[nodiscard]] constexpr Rect LeadingStrip(Rect rect, Side leading,
                                          int width) noexcept {
  const int stripWidth = width < rect.width ? width : rect.width;
  const int stripX =
      leading == Side::kLeft ? rect.x : rect.x + rect.width - stripWidth;
  return Rect{stripX, rect.y, stripWidth, rect.height};
}

}  // namespace fixedge::tab_edge
