// Geometry value types — one concern: plain rectangles, points and sizes,
// matching gfx::Rect's, gfx::Point's and gfx::Size's in-memory layouts so a
// pointer to one can be handed straight to Edge.
#pragma once
#include <cstdint>

namespace fixedge {

// gfx::Rect is four little-endian int32s {x, y, w, h} (see docs/re/split-view-abi.md).
struct Rect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  bool operator==(const Rect& other) const = default;
};

// gfx::Point is two int32s {x, y}; a Point* can be handed to Edge as a gfx::Point*.
struct Point {
  int32_t x = 0;
  int32_t y = 0;
};

// gfx::Size is two int32s {width, height}.
struct Size {
  int32_t width = 0;
  int32_t height = 0;
};

[[nodiscard]] inline constexpr bool Contains(const Rect& rect,
                                             const Point& point) {
  return point.x >= rect.x && point.x < rect.x + rect.width &&
         point.y >= rect.y && point.y < rect.y + rect.height;
}

// A rect with no area, which nothing can be inside.
[[nodiscard]] inline constexpr bool Empty(const Rect& rect) {
  return rect.width <= 0 || rect.height <= 0;
}

}  // namespace fixedge
