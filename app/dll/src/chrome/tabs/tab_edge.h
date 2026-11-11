// tab_edge.h — the geometry behind tab_shape.h's tab outline: which side a tab
// leads on (its favicon's, mirrored when Edge's UI runs right to left), an
// SkRRect's corner radii, and the strip that squares a split group's fill.
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

// Zero, a square corner, unless set.
struct CornerRadii {
  CornerRadius corners[kCornerCount];
};

// Painted over a rounded rectangle, squares its leading corners.
[[nodiscard]] constexpr Rect LeadingStrip(Rect rect, Side leading,
                                          int width) noexcept {
  const int stripWidth = width < rect.width ? width : rect.width;
  const int stripX =
      leading == Side::kLeft ? rect.x : rect.x + rect.width - stripWidth;
  return Rect{stripX, rect.y, stripWidth, rect.height};
}

}  // namespace edgeplus::tab_edge
