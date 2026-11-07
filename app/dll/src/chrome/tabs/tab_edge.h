// tab_edge.h — the geometry behind tab_shape.h's tab outline: the Cyberpunk
// kit's plate, square with its top trailing and bottom leading corners cut at
// 45 degrees (top right and bottom left, mirrored when Edge's UI runs right to
// left), which tabs are cut, and the strip that squares a split group's fill.
// Pure.
//
//   +-----------\        leading on the left
//   |            \
//   |            |
//    \           |
//     \----------+
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

// The cuts mark a tab of its own, and in a split the focused pane's tab only:
// the split's other members stay square, however many there are.
[[nodiscard]] constexpr bool IsPlateCut(bool isSplitMember,
                                        bool isFocusedPane) noexcept {
  return !isSplitMember || isFocusedPane;
}

// The two corners a plate is cut at: the top one on the side a tab trails to
// and the bottom one on its leading side.
[[nodiscard]] constexpr Corner TrailingTopCorner(Side leading) noexcept {
  return leading == Side::kLeft ? Corner::kUpperRight : Corner::kUpperLeft;
}

[[nodiscard]] constexpr Corner LeadingBottomCorner(Side leading) noexcept {
  return leading == Side::kLeft ? Corner::kLowerLeft : Corner::kLowerRight;
}

// The two cut corners keep the radius (tab_shape.h turns them into cuts) only
// when the plate is cut; the other two are square.
[[nodiscard]] constexpr CornerRadii RadiiFor(Side leading, float radius,
                                             bool isCut) noexcept {
  const CornerRadius round{radius, radius};
  const CornerRadius cutCorner = isCut ? round : CornerRadius{};
  CornerRadii radii;
  radii.corners[static_cast<size_t>(TrailingTopCorner(leading))] = cutCorner;
  radii.corners[static_cast<size_t>(LeadingBottomCorner(leading))] = cutCorner;
  return radii;
}

inline constexpr size_t kCutOutlinePointCount = 7;

// A vertical tab's plate: a polygon through these points in order, the last one
// the first again. `leg` is how far each cut reaches along both of its sides; a
// leg of 0 leaves a rectangle. Mirrored for a right-to-left UI by `leading`
// alone. (A horizontal tab is not mirrored here: Edge draws its outline left to
// right and flips the canvas under a right-to-left UI, so the cuts that
// tab_shape.h puts on the outline show mirrored.)
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
  // From either side, toward the tab's middle.
  const float inward = isLeadingOnLeft ? leg : -leg;
  return CutOutline{{{leadingX, top},
                     {trailingX - inward, top},
                     {trailingX, top + leg},
                     {trailingX, bottom},
                     {leadingX + inward, bottom},
                     {leadingX, bottom - leg},
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
