// drag.h — dragging a pane by its grip. One concern: the press -> move -> release
// session. Edge delivers the grip's mouse events through views (hooks.h detours
// views::Button's handlers), so views owns the mouse capture and every position
// arrives in the window's DIPs. Past the system drag threshold each move
// resolves the drop (dock::Resolve) and shows it (overlay); release commits the
// planned tree, and the transition animation carries the panes there. Escape or
// a lost capture cancels. UI thread only.
#pragma once
#include <windows.h>
#include <cmath>
#include <cstdlib>
#include "dock.h"
#include "edge.h"
#include "geometry.h"
#include "overlay.h"
#include "state.h"

namespace fixedge::drag {

// The browser window: owner of the preview, and the source of the DPI scale.
// Set once by control::Install.
inline HWND g_window = nullptr;

struct Session {
  bool isPressed = false;   // a grip is held, not yet past the drag threshold
  bool isDragging = false;  // past it: the preview follows the pointer
  void* mcv = nullptr;
  int pane = -1;      // the container index being dragged
  Point pressedAt{};  // window DIPs
  dock::Plan plan;    // what releasing now would do
};
inline Session g;

inline bool IsActive() {
  return g.isPressed || g.isDragging;
}

// --- coordinates: the window's DIPs <-> the split's DIPs <-> screen pixels ---

inline float Scale() {
  return static_cast<float>(GetDpiForWindow(g_window)) / 96.f;
}

inline Point RootToSplit(void* mcv, Point point) {
  edge::ConvertPoint(edge::ViewRoot(mcv), mcv, &point);
  return point;
}

// A rect in the split's DIPs -> screen pixels (for the preview window).
inline Rect SplitToScreen(void* mcv, const Rect& rect) {
  Point topLeft{rect.x, rect.y},
      bottomRight{rect.x + rect.width, rect.y + rect.height};
  void* root = edge::ViewRoot(mcv);
  edge::ConvertPoint(mcv, root, &topLeft);
  edge::ConvertPoint(mcv, root, &bottomRight);
  const float scale = Scale();
  POINT screenTopLeft{static_cast<LONG>(std::lround(topLeft.x * scale)),
                      static_cast<LONG>(std::lround(topLeft.y * scale))};
  POINT screenBottomRight{
      static_cast<LONG>(std::lround(bottomRight.x * scale)),
      static_cast<LONG>(std::lround(bottomRight.y * scale))};
  ClientToScreen(g_window, &screenTopLeft);
  ClientToScreen(g_window, &screenBottomRight);
  return Rect{screenTopLeft.x, screenTopLeft.y,
              screenBottomRight.x - screenTopLeft.x,
              screenBottomRight.y - screenTopLeft.y};
}

inline overlay::Style StyleOf(dock::Zone zone) {
  return zone == dock::Zone::kRootEdge ? overlay::Style::kRoot
         : zone == dock::Zone::kSwap   ? overlay::Style::kSwap
                                       : overlay::Style::kPane;
}

// --- the session -----------------------------------------------------------

// Escape / lost capture / teardown: end without committing.
inline void Cancel() {
  const bool wasDragging = g.isDragging;
  g = Session{};
  overlay::Hide();
  if (wasDragging) {
    Log("[drag] cancelled");
  }
}

// The grip of `pane` in `mcv` was pressed at `root` (window DIPs).
inline void Begin(void* mcv, int pane, Point root) {
  g = Session{};
  g.isPressed = true;
  g.mcv = mcv;
  g.pane = pane;
  g.pressedAt = root;
  Log("[drag] grip pressed on pane %d", pane);
}

// The pointer moved to `root` (window DIPs) with the grip held.
inline void Move(Point root) {
  if (!IsActive()) {
    return;
  }
  if (!g.isDragging) {
    const float scale = Scale();
    const int movedX =
        static_cast<int>(std::abs(root.x - g.pressedAt.x) * scale);
    const int movedY =
        static_cast<int>(std::abs(root.y - g.pressedAt.y) * scale);
    const bool isWithinDragThreshold = movedX < GetSystemMetrics(SM_CXDRAG) &&
                                       movedY < GetSystemMetrics(SM_CYDRAG);
    if (isWithinDragThreshold) {
      return;
    }
    g.isDragging = true;
    Log("[drag] dragging pane %d", g.pane);
  }
  const Node tree = geometry::LiveTree(g.mcv, edge::McvActiveSplitPanes(g.mcv));
  g.plan = dock::Resolve(tree, geometry::SplitArea(g.mcv),
                         RootToSplit(g.mcv, root), g.pane, g.plan);
  if (g.plan.zone == dock::Zone::kNone) {
    overlay::Hide();
  } else {
    overlay::Show(g_window, SplitToScreen(g.mcv, g.plan.preview),
                  StyleOf(g.plan.zone));
  }
  SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZEALL)));
}

// The grip was released: commit the planned drop (if any) and end the session.
inline void Release() {
  if (!IsActive()) {
    return;
  }
  const bool isDropping = g.isDragging && g.plan.zone != dock::Zone::kNone;
  void* mcv = g.mcv;
  dock::Plan plan = std::move(g.plan);
  const int pane = g.pane;
  g = Session{};
  overlay::Hide();
  if (!isDropping) {
    Log("[drag] released pane %d with no drop target", pane);
    return;
  }
  Trees()[mcv] = std::move(plan.result);
  edge::InvalidateLayout(mcv);
  edge::LayoutImmediately(
      mcv);  // a pure tree edit: force the re-tile (and its animation)
  Log("[drag] dropped pane %d: zone=%d target=%d dir=%d", pane,
      static_cast<int>(plan.zone), plan.target,
      static_cast<int>(plan.direction));
}

}  // namespace fixedge::drag
