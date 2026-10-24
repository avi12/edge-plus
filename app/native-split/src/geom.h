// geom.h — geometry value types laid out as gfx::Rect, gfx::Point and gfx::Size
// are, so a pointer to one can be handed straight to Edge.
#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>

namespace fixedge {

// docs/re/split-view-abi.md.
struct Rect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  bool operator==(const Rect& other) const = default;
};

struct Point {
  int32_t x = 0;
  int32_t y = 0;
};

// SkPoint / SkVector. Skia takes one by value in a single register, so a call
// passes it packed.
struct PointF {
  float x = 0;
  float y = 0;
  bool operator==(const PointF& other) const = default;
};

[[nodiscard]] inline constexpr uint64_t PackPoint(PointF point) noexcept {
  return std::bit_cast<uint64_t>(point);
}

[[nodiscard]] inline constexpr PointF UnpackPoint(uint64_t packed) noexcept {
  return std::bit_cast<PointF>(packed);
}

struct Size {
  int32_t width = 0;
  int32_t height = 0;
};

[[nodiscard]] inline constexpr bool Contains(const Rect& rect,
                                             const Point& point) noexcept {
  return point.x >= rect.x && point.x < rect.x + rect.width &&
         point.y >= rect.y && point.y < rect.y + rect.height;
}

[[nodiscard]] inline constexpr bool Empty(const Rect& rect) noexcept {
  return rect.width <= 0 || rect.height <= 0;
}

// The smallest rect holding both; an empty one adds nothing.
[[nodiscard]] inline constexpr Rect Union(const Rect& rect,
                                          const Rect& other) noexcept {
  if (Empty(rect)) {
    return other;
  }
  if (Empty(other)) {
    return rect;
  }
  const int32_t left = std::min(rect.x, other.x);
  const int32_t top = std::min(rect.y, other.y);
  const int32_t right = std::max(rect.x + rect.width, other.x + other.width);
  const int32_t bottom = std::max(rect.y + rect.height, other.y + other.height);
  return Rect{left, top, right - left, bottom - top};
}

[[nodiscard]] inline constexpr Rect GrowVertically(const Rect& rect,
                                                   int32_t reach) noexcept {
  return Rect{rect.x, rect.y - reach, rect.width, rect.height + 2 * reach};
}

}  // namespace fixedge
