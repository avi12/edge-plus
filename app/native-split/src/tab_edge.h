// tab_edge.h — where the selected vertical tab's spine goes and how a tab's
// leading edge is shaped. One concern: the pure geometry of the stripe spine.h
// lays along the selected tab's leading edge (the left one, the right one when
// Edge's UI runs right to left), the corner radii that keep that edge straight
// on every tab (tab_shape.h), and the stripe's colour for the theme's tone. No
// Edge, no Win32.
#pragma once
#include <cstddef>
#include "geom.h"
#include "theme_tone.h"

namespace fixedge::tab_edge {

// The stripe's neon magenta on a dark rail, and a deep magenta on a light one,
// where the neon would read at only 2.5:1.
inline constexpr theme_tone::Argb kStripeOnDark = 0xffff2bd6;
inline constexpr theme_tone::Argb kStripeOnLight = 0xffc8008f;

// A theme that says nothing of its tone gets the neon, as before there were two.
[[nodiscard]] constexpr theme_tone::Argb StripeColorFor(
    theme_tone::Tone tone) noexcept {
  return tone == theme_tone::Tone::kLight ? kStripeOnLight : kStripeOnDark;
}

// The side of the tab the spine marks: its leading edge, where the favicon is.
enum class Side {
  kLeft,
  kRight,
};

[[nodiscard]] constexpr Side LeadingSide(bool isRightToLeft) noexcept {
  return isRightToLeft ? Side::kRight : Side::kLeft;
}

// The stripe, in the same units as `tab`: the row's full height, flush on its
// leading edge.
[[nodiscard]] constexpr Rect StripeFor(Rect tab, Side side,
                                       int stripeWidth) noexcept {
  const int stripeX =
      side == Side::kLeft ? tab.x : tab.x + tab.width - stripeWidth;
  return Rect{stripeX, tab.y, stripeWidth, tab.height};
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

// A tab's corners with its leading side straight. Its trailing side is round,
// unless another member of a split sits against it in the same row: then the
// seam between the two is straight too.
[[nodiscard]] constexpr CornerRadii RadiiFor(
    Side leading, float radius, bool hasTrailingNeighbor) noexcept {
  const CornerRadius round{radius, radius};
  const CornerRadius square{};
  const CornerRadius trailing = hasTrailingNeighbor ? square : round;
  const bool isLeadingOnLeft = leading == Side::kLeft;
  CornerRadii radii;
  radii.corners[static_cast<size_t>(Corner::kUpperLeft)] =
      isLeadingOnLeft ? square : trailing;
  radii.corners[static_cast<size_t>(Corner::kLowerLeft)] =
      isLeadingOnLeft ? square : trailing;
  radii.corners[static_cast<size_t>(Corner::kUpperRight)] =
      isLeadingOnLeft ? trailing : square;
  radii.corners[static_cast<size_t>(Corner::kLowerRight)] =
      isLeadingOnLeft ? trailing : square;
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
