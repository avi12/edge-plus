// motion.h — the pure pane-transition math. One concern, zero Edge/Win32 deps,
// so it is unit-testable on the host: the easing curve, rect interpolation, and
// where a newly shown pane grows in from. anim.h drives it frame by frame.
#pragma once
#include <cmath>
#include <vector>
#include "geom.h"

namespace fixedge::motion {

// Transition length and curve: the snappy cubic-bezier(0.16, 1, 0.3, 1).
inline constexpr unsigned kDurationMs = 220;
inline constexpr double kX1 = 0.16, kY1 = 1.0, kX2 = 0.3, kY2 = 1.0;

// One axis of a CSS-style cubic bezier through (0,0), (a,b), (c,d), (1,1).
inline double Bezier(double t, double a, double c) {
  const double u = 1.0 - t;
  return 3 * u * u * t * a + 3 * u * t * t * c + t * t * t;
}

// Progress (0..1 linear time) -> eased progress. Solves x(t) = p by bisection
// (monotonic for x control points in [0,1]), then returns y(t).
inline double Ease(double p) {
  if (p <= 0.0) return 0.0;
  if (p >= 1.0) return 1.0;
  double lo = 0.0, hi = 1.0, t = p;
  for (int i = 0; i < 40; ++i) {
    t = (lo + hi) / 2;
    if (Bezier(t, kX1, kX2) < p) lo = t; else hi = t;
  }
  return Bezier(t, kY1, kY2);
}

inline bool Same(const Rect& a, const Rect& b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
inline bool Empty(const Rect& r) { return r.w <= 0 || r.h <= 0; }

inline int Mix(int a, int b, double t) {
  return a + static_cast<int>(std::lround((b - a) * t));
}
// Interpolate by eased progress `t`. Edges (not sizes) are mixed, so panes that
// tile at both ends stay seamless mid-flight.
inline Rect Lerp(const Rect& a, const Rect& b, double t) {
  const int l = Mix(a.x, b.x, t), top = Mix(a.y, b.y, t);
  const int r = Mix(a.x + a.w, b.x + b.w, t), bot = Mix(a.y + a.h, b.y + b.h, t);
  return Rect{l, top, r - l, bot - top};
}

// Where a pane that was not on screen grows in from. A new pane is carved out of
// a pane that was there (the one it split), so find the old rect holding its
// target's centre and collapse the target onto the edge it shares with that old
// rect on the side it was carved from: an add-right grows out of the old pane's
// right edge. With no such parent, it grows out of its own centre.
inline Rect EntryRect(const Rect& target, const std::vector<Rect>& before) {
  const int cx = target.x + target.w / 2, cy = target.y + target.h / 2;
  for (const Rect& p : before) {
    if (Empty(p) || cx < p.x || cx >= p.x + p.w || cy < p.y || cy >= p.y + p.h) continue;
    const int right = target.x + target.w, bottom = target.y + target.h;
    if (right == p.x + p.w && target.x > p.x) return Rect{right, target.y, 0, target.h};
    if (target.x == p.x && right < p.x + p.w) return Rect{target.x, target.y, 0, target.h};
    if (bottom == p.y + p.h && target.y > p.y) return Rect{target.x, bottom, target.w, 0};
    if (target.y == p.y && bottom < p.y + p.h) return Rect{target.x, target.y, target.w, 0};
    break;
  }
  return Rect{cx, cy, 0, 0};
}

}  // namespace fixedge::motion
