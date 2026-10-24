// drag.h — dragging a pane by its grip. Edge delivers the grip's mouse events
// through views (hooks.h detours views::Button's handlers), so views owns the
// mouse capture and every position arrives in the window's DIPs. UI thread.
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
#include "tabrows.h"
#include "state.h"

namespace fixedge::drag {

struct Session {
  bool isPressed = false;   // a grip is held, not yet past the drag threshold
  bool isDragging = false;  // past it: the preview follows the pointer
  void* mcv = nullptr;
  int pane = -1;
  Point pressedAt{};  // window DIPs
  dock::Plan plan;    // what releasing now would do
};
inline constinit Session g_session;

inline bool IsActive() {
  return g_session.isPressed || g_session.isDragging;
}

inline float Scale() {
  return static_cast<float>(GetDpiForWindow(channel::g_window)) /
         USER_DEFAULT_SCREEN_DPI;
}

inline Point RootToSplit(void* mcv, Point point) {
  edge::ConvertPoint(edge::ViewRoot(mcv), mcv, &point);
  return point;
}

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

// While the pane is held outside its split, the tab list shows it as a tab of
// its own and the rest of the split closed up.
inline void ShowDetachInTabList(void* mcv, int pane, dock::Zone zone) {
  const bool isGrownSplit =
      edge::McvActiveSplitPanes(mcv) > tabrows::kPairedMembers;
  int tabIndex = -1;
  if (zone == dock::Zone::kDetach && isGrownSplit) {
    void* contents =
        edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
    tabIndex = edge::IndexOfWebContents(edge::McvTabStripModel(mcv), contents);
  }
  tabrows::SetDetachPreview(tabIndex);
}

// While the pane is held outside its split, the split closes up without it.
inline void HoldOut(void* mcv, int pane) {
  const bool isChanged = g_heldOut.mcv != mcv || g_heldOut.pane != pane;
  if (!isChanged) {
    return;
  }
  void* laidOut = pane >= 0 ? mcv : g_heldOut.mcv;
  g_heldOut = pane >= 0 ? HeldOutPane{mcv, pane} : HeldOutPane{};
  if (laidOut) {
    edge::InvalidateLayout(laidOut);
    edge::LayoutImmediately(laidOut);
  }
}

inline void PutBack() {
  HoldOut(nullptr, -1);
}

inline void Cancel() {
  const bool wasDragging = g_session.isDragging;
  g_session = Session{};
  overlay::Hide();
  PutBack();
  tabrows::SetDetachPreview(tabrows::kNoDetachPreview);
  if (wasDragging) {
    Log("[drag] cancelled");
  }
}

inline void Begin(void* mcv, int pane, Point rootLocation) {
  g_session = Session{};
  g_session.isPressed = true;
  g_session.mcv = mcv;
  g_session.pane = pane;
  g_session.pressedAt = rootLocation;
  Log("[drag] grip pressed on pane %d", pane);
}

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
  ShowDetachInTabList(g_session.mcv, g_session.pane, g_session.plan.zone);
  // Leaving the split is shown by the split and the tab list themselves, not
  // by an outline.
  const bool isLeavingSplit = g_session.plan.zone == dock::Zone::kDetach;
  if (isLeavingSplit) {
    HoldOut(g_session.mcv, g_session.pane);
  } else {
    PutBack();
  }
  const bool hasDropTarget =
      g_session.plan.zone != dock::Zone::kNone && !isLeavingSplit;
  if (hasDropTarget) {
    overlay::Show(channel::g_window,
                  SplitToScreen(g_session.mcv, g_session.plan.preview),
                  StyleOf(g_session.plan.zone));
  } else {
    overlay::Hide();
  }
  SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZEALL)));
}

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
  // Before any real edit, whose layout must see the whole split.
  g_heldOut = HeldOutPane{};
  const bool isDetaching = isDropping && plan.zone == dock::Zone::kDetach;
  // A real detach lays the list out itself; any other end puts it back now.
  tabrows::SetDetachPreview(tabrows::kNoDetachPreview, !isDetaching);
  if (!isDropping) {
    Log("[drag] released pane %d with no drop target", pane);
    return;
  }
  if (isDetaching) {
    const bool hasDetached = split_ops::DetachPane(mcv, pane);
    if (hasDetached) {
      repaint::ScheduleAll();
    } else {
      edge::InvalidateLayout(mcv);
      edge::LayoutImmediately(mcv);
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
