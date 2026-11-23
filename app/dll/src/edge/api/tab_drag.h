// tab_drag.h — a tab drag (TabDragController) and its drag context: the tabs
// it carries, and starting, moving and ending one.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/geom.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline bool IsDraggingTabs(void* controller) {
  return Field<int32_t>(controller, edge155::off::TABDRAG_state) ==
         edge155::off::TABDRAG_state_dragging_tabs;
}
inline size_t DraggedTabCount(void* controller) {
  return Field<uint64_t>(controller, edge155::off::TABDRAG_drag_data_count);
}
// The WebContents of dragged tab `i` (< DraggedTabCount).
inline void* DraggedContents(void* controller, size_t i) {
  auto* entries = Field<uint8_t*>(controller, edge155::off::TABDRAG_drag_data);
  return Field<void*>(entries + i * edge155::off::TABDRAGDATA_size,
                      edge155::off::TABDRAGDATA_contents);
}
// The tab strip the drag is attached to (a TabDragContext view).
inline void* DragContext(void* controller) {
  return Field<void*>(controller, edge155::off::TABDRAG_attached_context);
}
inline void* DragContextTabStripModel(void* context) {
  using ModelFn = void*(__fastcall*)(void*);
  return reinterpret_cast<ModelFn>(VirtualFunction(
      context, edge155::off::DRAGCONTEXT_vtbl_GetTabStripModel))(context);
}
inline void* DragContextTabFor(void* context, void* webContents) {
  using TabFn = void*(__fastcall*)(void*, void*);
  return reinterpret_cast<TabFn>(VirtualFunction(
      context, edge155::off::DRAGCONTEXT_vtbl_GetTabForContents))(context,
                                                                  webContents);
}
// The drag running in this strip, or null.
inline void* DragContextController(void* context) {
  using ControllerFn = void*(__fastcall*)(void*);
  return reinterpret_cast<ControllerFn>(VirtualFunction(
      context, edge155::off::DRAGCONTEXT_vtbl_GetDragController))(context);
}
// Ends the strip's drag as its own mouse handlers do; frees the controller.
inline void DragContextEndDrag(void* context, int reason) {
  using EndFn = void(__fastcall*)(void*, int);
  reinterpret_cast<EndFn>(VirtualFunction(
      context, edge155::off::DRAGCONTEXT_vtbl_EndDrag))(context, reason);
}

// The TabStrip of a window's tab list, vertical or not.
inline void* BrowserViewTabStrip(void* browserView) {
  using TabStripFn = void*(__fastcall*)(void*);
  return Callable<TabStripFn>(edge155::rva::BrowserView_tabstrip)(browserView);
}
// The tab strip of a window's tab list as a TabDragContext view, or null.
inline void* BrowserViewDragContext(void* browserView) {
  const void* tabStrip = BrowserViewTabStrip(browserView);
  using ContextFn = void*(__fastcall*)(const void*);
  return tabStrip ? Callable<ContextFn>(edge155::rva::TabStrip_GetDragContext)(
                        tabStrip)
                  : nullptr;
}
// Ends the strip's running animations, its tabs laid out where they go.
inline void TabStripStopAnimating(void* tabStrip) {
  using StopFn = void(__fastcall*)(void*);
  Callable<StopFn>(edge155::rva::TabStrip_StopAnimating)(tabStrip);
}

// Starts Edge's own drag of the selected tabs from `tabView`, grabbed at
// `grabPoint` (the tab's coordinates), exactly as a left press on the tab does:
// Tab::OnMousePressed hands its TabSlotController a press event and the
// selection.
inline void StartTabDrag(void* tabView, Point grabPoint) {
  void* controller = Field<void*>(tabView, edge155::off::TABVIEW_controller);
  if (!controller) {
    return;
  }
  alignas(16) uint8_t pointer[edge155::off::POINTERDETAILS_size] = {};
  using CtorFn = void*(__fastcall*)(void*);
  Callable<CtorFn>(edge155::rva::PointerDetails_ctor)(pointer);
  int64_t now = 0;
  using NowFn = void*(__fastcall*)(int64_t*);
  Callable<NowFn>(edge155::rva::TimeTicks_Now)(&now);
  alignas(16) uint8_t event[edge155::off::MOUSEEVENT_size] = {};
  using EventFn = void*(__fastcall*)(void*, int, const Point*, const Point*,
                                     int64_t, int, int, const void*);
  Callable<EventFn>(edge155::rva::MouseEvent_ctor)(
      event, edge155::off::EVENT_type_mouse_pressed, &grabPoint, &grabPoint,
      now, edge155::off::EVENT_flag_left_mouse_button,
      edge155::off::EVENT_flag_left_mouse_button, pointer);
  // MaybeStartDrag destroys its by-value selection itself.
  alignas(16) uint8_t selection[edge155::off::LISTSELECTIONMODEL_size] = {};
  using SelectionFn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<SelectionFn>(VirtualFunction(
      controller, edge155::off::SLOTCONTROLLER_vtbl_GetSelectionModel))(
      controller, selection);
  using StartFn = void(__fastcall*)(void*, void*, const void*, void*);
  reinterpret_cast<StartFn>(VirtualFunction(
      controller, edge155::off::SLOTCONTROLLER_vtbl_MaybeStartDrag))(
      controller, tabView, event, selection);
  using DtorFn = void(__fastcall*)(void*);
  Callable<DtorFn>(edge155::rva::Event_dtor)(event);
}

// Gives the strip the mouse capture, so the drag's moves and release reach it.
inline void TabDragSetCapture(void* controller, void* context) {
  using CaptureFn = bool(__fastcall*)(void*, void*);
  Callable<CaptureFn>(edge155::rva::TabDragController_SetCapture)(controller,
                                                                  context);
}

// A pointer move of the drag, in screen DIPs, as the strip's OnMouseDragged
// delivers it.
inline void TabDragMoveTo(void* controller, Point screenPoint) {
  using DragFn = int(__fastcall*)(void*, const Point*);
  Callable<DragFn>(edge155::rva::TabDragController_Drag)(controller,
                                                         &screenPoint);
}

}  // namespace edgeplus::edge
