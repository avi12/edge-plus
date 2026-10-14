// Geometry value types — one concern: plain rectangles, matching gfx::Rect's
// in-memory layout so a Rect* can be handed straight to Edge as a const gfx::Rect&.
#pragma once
#include <cstdint>

namespace fixedge {

// gfx::Rect is four little-endian int32s {x, y, w, h} (see docs/re/split-view-abi.md).
struct Rect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
};

// gfx::Point is two int32s {x, y}; a Point* can be handed to Edge as a gfx::Point*.
struct Point {
  int32_t x = 0;
  int32_t y = 0;
};

inline bool Contains(const Rect& r, const Point& p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

}  // namespace fixedge
