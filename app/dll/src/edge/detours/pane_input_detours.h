// pane_input_detours.h — the detours on the mouse input of what a pane wears:
// views::Button's handlers for the drag grip (drag.h), Edge's resize area's
// for the dividers between panes (dividers.h), and the hover controls' X,
// which closes its own pane in a split of three or more.
#pragma once
#include "chrome/pane/dividers.h"
#include "chrome/pane/drag.h"
#include "chrome/pane/grip.h"
#include "chrome/pane/menu.h"
#include "chrome/tabs/tabdrag.h"
#include "core/channel.h"
#include "edge/api/events.h"
#include "edge/api/split_controls.h"
#include "edge/api/split_view.h"
#include "edge/api/views.h"
#include "layout/geometry.h"
#include "split/split_ops.h"

namespace edgeplus::hooks {

inline constexpr int kDoubleClickCount = 2;

// views::Button's mouse handlers, shared by every button in Edge. A grip has no
// handler of its own (grip.h): its press is claimed, so views keeps sending that
// button the drags and the release with its capture.
using ButtonPressed_t = bool(__fastcall*)(void*, const void*);
using ButtonReleased_t = void(__fastcall*)(void*, const void*);
using ButtonCaptureLost_t = void(__fastcall*)(void*);
inline constinit ButtonPressed_t o_pressed = nullptr;
inline constinit ButtonPressed_t o_dragged = nullptr;
inline constinit ButtonReleased_t o_released = nullptr;
inline constinit ButtonCaptureLost_t o_capture_lost = nullptr;

// views::EdgeTwoDimensionalResizeArea's mouse handlers: Edge's own divider
// class, shared by its two-pane handle and our per-seam dividers (dividers.h).
// A press on one of ours is claimed, so views keeps sending it the drags and the
// release.
using DividerEvent_t = bool(__fastcall*)(void*, const void*);
using DividerReleased_t = void(__fastcall*)(void*, const void*);
using DividerCaptureLost_t = void(__fastcall*)(void*);
inline constinit DividerEvent_t o_divider_pressed = nullptr;
inline constinit DividerEvent_t o_divider_dragged = nullptr;
inline constinit DividerReleased_t o_divider_released = nullptr;
inline constinit DividerCaptureLost_t o_divider_capture_lost = nullptr;

inline bool __fastcall h_divider_pressed(void* view, const void* event) {
  void* mcv = nullptr;
  Seam seam;
  if (!dividers::Locate(view, &mcv, &seam)) {
    return o_divider_pressed(view, event);
  }
  const int panes = edge::McvActiveSplitPanes(mcv);
  dividers::Begin(mcv, seam, geometry::LiveTree(mcv, panes),
                  edge::EventRootLocation(event));
  return true;
}

inline bool __fastcall h_divider_dragged(void* view, const void* event) {
  void* mcv = nullptr;
  Seam seam;
  if (!dividers::Locate(view, &mcv, &seam)) {
    return o_divider_dragged(view, event);
  }
  dividers::Move(edge::EventRootLocation(event));
  return true;
}

inline void __fastcall h_divider_released(void* view, const void* event) {
  void* mcv = nullptr;
  Seam seam;
  if (!dividers::Locate(view, &mcv, &seam)) {
    o_divider_released(view, event);
    return;
  }
  dividers::End();
  // As Edge's own handle does for its two panes.
  const bool isDoubleClick = edge::MouseClickCount(event) == kDoubleClickCount;
  if (isDoubleClick) {
    dividers::ResetToEqual(
        mcv, seam, geometry::LiveTree(mcv, edge::McvActiveSplitPanes(mcv)));
  }
}

inline void __fastcall h_divider_capture_lost(void* view) {
  void* mcv = nullptr;
  Seam seam;
  if (!dividers::Locate(view, &mcv, &seam)) {
    o_divider_capture_lost(view);
    return;
  }
  dividers::End();
}

inline bool __fastcall h_pressed(void* button, const void* event) {
  void* mcv = nullptr;
  int pane = -1;
  const bool isGrip = grip::Locate(button, &mcv, &pane);
  if (!isGrip) {
    return o_pressed(button, event);
  }
  drag::Begin(mcv, pane, edge::EventRootLocation(event));
  return true;
}

inline bool __fastcall h_dragged(void* button, const void* event) {
  if (grip::IsGrip(button)) {
    tabdrag::OnGripDragged(edge::EventRootLocation(event));
    // Handed to the tab list, which now holds the capture: the grip sees no
    // release, so it lets go here instead of staying drawn pressed.
    if (!drag::IsActive()) {
      o_capture_lost(button);
      return true;
    }
  }
  return o_dragged(button, event);
}

inline void __fastcall h_released(void* button, const void* event) {
  if (grip::IsGrip(button)) {
    drag::Release();
  }
  // A menu arrow released over itself is a click. It runs once this event is
  // done, since closing the menu destroys the arrow.
  const int arrow = menu::ArrowIndexOf(button);
  const bool isArrowClick =
      arrow != menu::kNoArrow && edge::ViewIsMouseHovered(button);
  o_released(button, event);
  if (isArrowClick) {
    channel::Post(channel::CodeOf(channel::Command::kMenuArrowBase,
                                  menu::kMenuItems[arrow].direction));
  }
}

inline void __fastcall h_capture_lost(void* button) {
  if (grip::IsGrip(button)) {
    drag::Cancel();
  }
  o_capture_lost(button);
}

// ControlButtons::HandleButtonClick(bar, button): Edge's X on pane 2+ closes the
// split's last tab and dissolves the split, so in a split of three or more the X
// closes its own pane instead.
using ButtonClick_t = void(__fastcall*)(void*, void*);
inline constinit ButtonClick_t o_button_click = nullptr;
inline void __fastcall h_button_click(void* bar, void* button) {
  const bool isCloseButton = button == edge::ControlButtonsCloseButton(bar);
  void* mcv = nullptr;
  int pane = -1;
  const bool isClosingOwnPane = isCloseButton &&
                                grip::LocateBar(bar, &mcv, &pane) &&
                                split_ops::ClosePane(mcv, pane);
  if (isClosingOwnPane) {
    return;
  }
  o_button_click(bar, button);
}

}  // namespace edgeplus::hooks
