// motion.h — the pane-transition math anim.h drives frame by frame. Pure.
#pragma once
#include <cmath>
#include <span>
#include "geom.h"

namespace fixedge::motion {

// cubic-bezier(0.16, 1, 0.3, 1), its control points named as CSS names them.
inline constexpr unsigned kDurationMs = 220;
inline constexpr double kX1 = 0.16;
inline constexpr double kY1 = 1.0;
inline constexpr double kX2 = 0.3;
inline constexpr double kY2 = 1.0;
// An error of about 1e-12, far below a pixel.
inline constexpr int kBisectionSteps = 40;

// One axis of a CSS-style cubic bezier running from 0 to 1.
[[nodiscard]] inline constexpr double Bezier(double curveTime,
                                             double firstControl,
                                             double secondControl) noexcept {
  const double remaining = 1.0 - curveTime;
  return 3 * remaining * remaining * curveTime * firstControl +
         3 * remaining * curveTime * curveTime * secondControl +
         curveTime * curveTime * curveTime;
}

// Solves x(t) = progress by bisection (monotonic for x control points in
// [0,1]), then returns y(t).
[[nodiscard]] inline constexpr double Ease(double progress) noexcept {
  if (progress <= 0.0) {
    return 0.0;
  }
  if (progress >= 1.0) {
    return 1.0;
  }
  double low = 0.0;
  double high = 1.0;
  double curveTime = progress;
  for (int i = 0; i < kBisectionSteps; i++) {
    curveTime = (low + high) / 2;
    const bool isShortOfProgress = Bezier(curveTime, kX1, kX2) < progress;
    if (isShortOfProgress) {
      low = curveTime;
    } else {
      high = curveTime;
    }
  }
  return Bezier(curveTime, kY1, kY2);
}

[[nodiscard]] inline constexpr bool Same(const Rect& first,
                                         const Rect& second) noexcept {
  return first.x == second.x && first.y == second.y &&
         first.width == second.width && first.height == second.height;
}
// anim.h calls it as motion::Empty.
using fixedge::Empty;

[[nodiscard]] inline int Mix(int from, int to, double progress) noexcept {
  return from + static_cast<int>(std::lround((to - from) * progress));
}
// Edges (not sizes) are mixed, so panes that tile at both ends stay seamless
// mid-flight.
[[nodiscard]] inline Rect Lerp(const Rect& from, const Rect& to,
                               double progress) noexcept {
  const int left = Mix(from.x, to.x, progress);
  const int top = Mix(from.y, to.y, progress);
  const int right = Mix(from.x + from.width, to.x + to.width, progress);
  const int bottom = Mix(from.y + from.height, to.y + to.height, progress);
  return Rect{left, top, right - left, bottom - top};
}

// A new pane is carved out of the old pane holding its centre, so it grows out
// of the edge it was carved from (an add-right grows out of the old pane's
// right edge); with no such parent, out of its own centre.
[[nodiscard]] inline Rect EntryRect(const Rect& target,
                                    std::span<const Rect> before) noexcept {
  const Point centre{target.x + target.width / 2, target.y + target.height / 2};
  for (const Rect& parent : before) {
    const bool isParent = !Empty(parent) && Contains(parent, centre);
    if (!isParent) {
      continue;
    }
    const int right = target.x + target.width;
    const int bottom = target.y + target.height;
    const int parentRight = parent.x + parent.width;
    const int parentBottom = parent.y + parent.height;
    const bool isCarvedFromRight = right == parentRight && target.x > parent.x;
    if (isCarvedFromRight) {
      return Rect{right, target.y, 0, target.height};
    }
    const bool isCarvedFromLeft = target.x == parent.x && right < parentRight;
    if (isCarvedFromLeft) {
      return Rect{target.x, target.y, 0, target.height};
    }
    const bool isCarvedFromBottom =
        bottom == parentBottom && target.y > parent.y;
    if (isCarvedFromBottom) {
      return Rect{target.x, bottom, target.width, 0};
    }
    const bool isCarvedFromTop = target.y == parent.y && bottom < parentBottom;
    if (isCarvedFromTop) {
      return Rect{target.x, target.y, target.width, 0};
    }
    break;
  }
  return Rect{centre.x, centre.y, 0, 0};
}

}  // namespace fixedge::motion
