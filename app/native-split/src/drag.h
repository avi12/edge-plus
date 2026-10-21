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
#include "channel.h"
#include "dock.h"
#include "edge.h"
#include "geometry.h"
#include "overlay.h"
#include "repaint.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::drag {

struct Session {
  bool isPressed = false;   // a grip is held, not yet past the drag threshold
  bool isDragging = false;  // past it: the preview follows the pointer
  void* mcv = nullptr;
  int pane = -1;      // the container index being dragged
  Point pressedAt{};  // window DIPs
  dock::Plan plan;    // what releasing now would do
};
inline constinit Session g_session;

inline bool IsActive() {
  return g_session.isPressed || g_session.isDragging;
}

// --- coordinates: the window's DIPs <-> the split's DIPs <-> screen pixels ---

inline float Scale() {
  return static_cast<float>(GetDpiForWindow(channel::g_window)) /
         USER_DEFAULT_SCREEN_DPI;
}

inline Point RootToSplit(void* mcv, Point point) {
  edge::ConvertPoint(edge::ViewRoot(mcv), mcv, &point);
  return point;
}

// A rect in the split's DIPs -> screen pixels (for the preview window).
inline Rect SplitToScreen(void* mcv, const Rect& rect) {
  Point topLeft{rect.x, rect.y};
  Point bottomRight{rect.x + rect.width, rect.y + rect.height};
  void* root = edge::ViewRoot(mcv);
  edge::ConvertPoint(mcv, root, &topLeft);
  edge::ConvertPoint(mcv, root, &bottomRight);
  const float scale = Scale();
  POINT screenTopLeft{
      static_cast<LONG>(std::lround(static_cast<float>(topLeft.x) * scale)),
      static_cast<LONG>(std::lround(static_cast<float>(topLeft.y) * scale))};
  POINT screenBottomRight{
      static_cast<LONG>(std::lround(static_cast<float>(bottomRight.x) * scale)),
      static_cast<LONG>(
          std::lround(static_cast<float>(bottomRight.y) * scale))};
  ClientToScreen(channel::g_window, &screenTopLeft);
  ClientToScreen(channel::g_window, &screenBottomRight);
  return Rect{screenTopLeft.x, screenTopLeft.y,
              screenBottomRight.x - screenTopLeft.x,
              screenBottomRight.y - screenTopLeft.y};
}

inline constexpr overlay::Style StyleOf(dock::Zone zone) noexcept {
  switch (zone) {
    case dock::Zone::kRootEdge:
      return overlay::Style::kRoot;
    case dock::Zone::kSwap:
    case dock::Zone::kDetach:
      return overlay::Style::kSwap;
    default:
      return overlay::Style::kPane;
  }
}

// --- the session -----------------------------------------------------------

// Escape / lost capture / teardown: end without committing.
inline void Cancel() {
  const bool wasDragging = g_session.isDragging;
  g_session = Session{};
  overlay::Hide();
  if (wasDragging) {
    Log("[drag] cancelled");
  }
}

// The grip of `pane` in `mcv` was pressed at `rootLocation` (window DIPs).
inline void Begin(void* mcv, int pane, Point rootLocation) {
  g_session = Session{};
  g_session.isPressed = true;
  g_session.mcv = mcv;
  g_session.pane = pane;
  g_session.pressedAt = rootLocation;
  Log("[drag] grip pressed on pane %d", pane);
}

// The pointer moved to `rootLocation` (window DIPs) with the grip held.
inline void Move(Point rootLocation) {
  if (!IsActive()) {
    return;
  }
  if (!g_session.isDragging) {
    const float scale = Scale();
    const int movedX = static_cast<int>(
        static_cast<float>(std::abs(rootLocation.x - g_session.pressedAt.x)) *
        scale);
    const int movedY = static_cast<int>(
        static_cast<float>(std::abs(rootLocation.y - g_session.pressedAt.y)) *
        scale);
    const bool isWithinDragThreshold = movedX < GetSystemMetrics(SM_CXDRAG) &&
                                       movedY < GetSystemMetrics(SM_CYDRAG);
    if (isWithinDragThreshold) {
      return;
    }
    g_session.isDragging = true;
    Log("[drag] dragging pane %d", g_session.pane);
  }
  const Node tree = geometry::LiveTree(
      g_session.mcv, edge::McvActiveSplitPanes(g_session.mcv));
  g_session.plan = dock::Resolve(tree, geometry::SplitArea(g_session.mcv),
                                 RootToSplit(g_session.mcv, rootLocation),
                                 g_session.pane, g_session.plan);
  const bool hasDropTarget = g_session.plan.zone != dock::Zone::kNone;
  if (hasDropTarget) {
    overlay::Show(channel::g_window,
                  SplitToScreen(g_session.mcv, g_session.plan.preview),
                  StyleOf(g_session.plan.zone));
  } else {
    overlay::Hide();
  }
  SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZEALL)));
}

// The grip was released: commit the planned drop (if any) and end the session.
inline void Release() {
  if (!IsActive()) {
    return;
  }
  const bool isDropping =
      g_session.isDragging && g_session.plan.zone != dock::Zone::kNone;
  void* mcv = g_session.mcv;
  dock::Plan plan = std::move(g_session.plan);
  const int pane = g_session.pane;
  g_session = Session{};
  overlay::Hide();
  if (!isDropping) {
    Log("[drag] released pane %d with no drop target", pane);
    return;
  }
  const bool isDetaching = plan.zone == dock::Zone::kDetach;
  if (isDetaching) {
    const bool hasDetached = split_ops::DetachPane(mcv, pane);
    if (hasDetached) {
      repaint::ScheduleAll();
    }
    Log("[drag] pane %d %s", pane,
        hasDetached ? "detached as a tab" : "could not be detached");
    return;
  }
  Trees()[mcv] = std::move(plan.result);
  edge::InvalidateLayout(mcv);
  // A pure tree edit: force the re-tile (and its animation).
  edge::LayoutImmediately(mcv);
  repaint::ScheduleAll();
  Log("[drag] dropped pane %d: zone=%d target=%d dir=%d", pane,
      static_cast<int>(plan.zone), plan.target,
      static_cast<int>(plan.direction));
}

}  // namespace fixedge::drag
