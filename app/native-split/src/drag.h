// drag.h — dragging a pane by its grip. One concern: the press -> move -> release
// session. A press on a grip captures the mouse; past the system drag threshold
// each move resolves the drop (dock::Resolve) and shows it (overlay); release
// commits the planned tree, and the transition animation carries the panes
// there. Escape or a lost capture cancels. Driven by the window procedure
// (control.h); coordinates arrive as client pixels. UI thread only.
#pragma once
#include <windows.h>
#include <windowsx.h>
#include <cmath>
#include <cstdlib>
#include "dock.h"
#include "edge.h"
#include "geometry.h"
#include "grip.h"
#include "overlay.h"
#include "state.h"

namespace fixedge::drag {

struct Session {
  bool pressed = false;   // a grip is held, not yet past the drag threshold
  bool dragging = false;  // past it: the preview follows the pointer
  HWND hwnd = nullptr;
  void* mcv = nullptr;
  int pane = -1;          // the container index being dragged
  POINT down{};
  dock::Plan plan;        // what releasing now would do
};
inline Session g;

inline bool Active() { return g.pressed || g.dragging; }
inline bool Dragging() { return g.dragging; }

// --- coordinates: client pixels <-> the window's DIPs <-> the split's DIPs -----

inline float Scale(HWND hwnd) { return static_cast<float>(GetDpiForWindow(hwnd)) / 96.f; }

inline Point ClientToRoot(HWND hwnd, LPARAM lparam) {
  const float s = Scale(hwnd);
  return Point{static_cast<int32_t>(std::floor(GET_X_LPARAM(lparam) / s)),
               static_cast<int32_t>(std::floor(GET_Y_LPARAM(lparam) / s))};
}

inline Point RootToSplit(void* mcv, Point p) {
  edge::ConvertPoint(edge::ViewRoot(mcv), mcv, &p);
  return p;
}

// A rect in the split's DIPs -> screen pixels (for the preview window).
inline Rect SplitToScreen(HWND hwnd, void* mcv, const Rect& r) {
  Point a{r.x, r.y}, b{r.x + r.w, r.y + r.h};
  void* root = edge::ViewRoot(mcv);
  edge::ConvertPoint(mcv, root, &a);
  edge::ConvertPoint(mcv, root, &b);
  const float s = Scale(hwnd);
  POINT pa{static_cast<LONG>(std::lround(a.x * s)), static_cast<LONG>(std::lround(a.y * s))};
  POINT pb{static_cast<LONG>(std::lround(b.x * s)), static_cast<LONG>(std::lround(b.y * s))};
  ClientToScreen(hwnd, &pa);
  ClientToScreen(hwnd, &pb);
  return Rect{pa.x, pa.y, pb.x - pa.x, pb.y - pa.y};
}

inline overlay::Style StyleOf(dock::Zone zone) {
  return zone == dock::Zone::kRootEdge ? overlay::Style::kRoot
         : zone == dock::Zone::kSwap   ? overlay::Style::kSwap
                                       : overlay::Style::kPane;
}

// --- the session -----------------------------------------------------------

// End the session. Safe re-entrantly: it is cleared before ReleaseCapture (which
// sends WM_CAPTURECHANGED back to us).
inline void End() {
  const HWND hwnd = g.hwnd;
  g = Session{};
  overlay::Hide();
  if (hwnd && GetCapture() == hwnd) ReleaseCapture();
}

// Escape / lost capture: end without committing.
inline void Cancel() {
  const bool was_dragging = g.dragging;
  End();
  if (was_dragging) Log("[drag] cancelled");
}

// WM_LBUTTONDOWN: start a session if the press is on a pane's grip.
inline bool OnButtonDown(HWND hwnd, LPARAM lparam) {
  void* mcv = edge::McvForWindow(hwnd);
  if (!mcv || edge::McvCount(mcv) <= 2) return false;
  const int pane = grip::PaneAt(mcv, ClientToRoot(hwnd, lparam));
  if (pane < 0) return false;
  g = Session{};
  g.pressed = true;
  g.hwnd = hwnd;
  g.mcv = mcv;
  g.pane = pane;
  g.down = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  SetCapture(hwnd);
  Log("[drag] grip pressed on pane %d", pane);
  return true;
}

// WM_MOUSEMOVE: past the threshold, re-resolve the drop and move the preview.
inline bool OnMouseMove(HWND hwnd, LPARAM lparam) {
  if (!Active()) return false;
  if (!g.dragging) {
    const int dx = std::abs(GET_X_LPARAM(lparam) - g.down.x);
    const int dy = std::abs(GET_Y_LPARAM(lparam) - g.down.y);
    if (dx < GetSystemMetrics(SM_CXDRAG) && dy < GetSystemMetrics(SM_CYDRAG)) return true;
    g.dragging = true;
    Log("[drag] dragging pane %d", g.pane);
  }
  const Node tree = geometry::LiveTree(g.mcv, edge::McvActiveSplitPanes(g.mcv));
  const Point at = RootToSplit(g.mcv, ClientToRoot(hwnd, lparam));
  g.plan = dock::Resolve(tree, geometry::SplitArea(g.mcv), at, g.pane, g.plan);
  if (g.plan.zone == dock::Zone::kNone) {
    overlay::Hide();
  } else {
    overlay::Show(hwnd, SplitToScreen(hwnd, g.mcv, g.plan.preview), StyleOf(g.plan.zone));
  }
  return true;
}

// WM_LBUTTONUP: commit the planned drop (if any) and end the session.
inline bool OnButtonUp(HWND, LPARAM) {
  if (!Active()) return false;
  const bool commit = g.dragging && g.plan.zone != dock::Zone::kNone;
  void* mcv = g.mcv;
  dock::Plan plan = std::move(g.plan);
  const int pane = g.pane;
  End();  // releases capture + hides the preview
  if (!commit) {
    Log("[drag] released pane %d with no drop target", pane);
    return true;
  }
  Trees()[mcv] = std::move(plan.result);
  edge::InvalidateLayout(mcv);
  edge::LayoutImmediately(mcv);  // a pure tree edit: force the re-tile (and its animation)
  Log("[drag] dropped pane %d: zone=%d target=%d dir=%d", pane, static_cast<int>(plan.zone),
      plan.target, static_cast<int>(plan.dir));
  return true;
}

}  // namespace fixedge::drag
