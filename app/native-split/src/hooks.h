// hooks.h — MinHook plumbing and the detours, wiring the modules together.
// One concern: intercept Edge's split methods and delegate to panes/geometry/menu.
// Every detour is gated so a stock (<=2) split runs the original untouched; panes
// grow only on an explicit directional-add command (menu.h, control.h), never
// automatically.
#pragma once
#include <cstdint>
#include <cstring>
#include "MinHook.h"
#include "edge.h"
#include "panes.h"
#include "geometry.h"
#include "collapse.h"
#include "menu.h"
#include "control.h"
#include "protocol_handlers.h"
#include "dividers.h"
#include "tabrows.h"
#include "drag.h"
#include "grip.h"
#include "state.h"
#include "assistant.h"
#include "find.h"
#include "theme_follow.h"

namespace fixedge::hooks {

using namespace fixedge::edge155;

// Each 2-pane CHECK that InstallAll NOPs is a `jcc rel32` (0F 8x + a 4-byte
// offset; see offsets.h).
inline constexpr size_t kCheckJumpLength = 6;
using edge::kLiftedIndexCap;

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. For a grown
// split (index >= the current container count) build the missing container(s)
// first, then run the original. The index<2 guard is lifted at install (see the
// safety patches below). A normal 2-pane split only sees index 0/1, so it is
// unaffected.
using SetWc_t = void*(__fastcall*)(void*, void*, uint32_t);
inline SetWc_t o_setwc = nullptr;
inline void* __fastcall h_setwc(void* mcv, void* webContents, uint32_t index) {
  const bool shouldBuildContainers = index >= edge::McvCount(mcv);
  if (shouldBuildContainers) {
    Log("[grow] feeder wants index %u; count %llu - building container(s)",
        index, static_cast<unsigned long long>(edge::McvCount(mcv)));
    while (edge::McvCount(mcv) <= index) {
      const bool isAdded = panes::AddContainer(mcv);
      if (!isAdded) {
        break;
      }
    }
  }
  void* result = o_setwc(mcv, webContents, index);
  // Every pane in a split is on-screen, but Edge only force-paints the ones it
  // natively tracks; a fed background member stays blank until shown. Show it so
  // panes 2+ (and re-fed members) actually paint. Harmless for the active tab.
  if (webContents) {
    edge::WebContentsWasShown(webContents);
    // Edge only watches the first and last member for hover; watch every pane.
    edge::ListenSplitInput(webContents);
  }
  // Pane 3+ needs a hover bar, every pane of a grown split a drag grip, and
  // every seam a divider.
  panes::EnsureControlButtons(mcv, static_cast<int>(index));
  grip::EnsureGrips(mcv);
  dividers::Ensure(mcv);
  return result;
}

// LayoutManagerBase::CalculateProposedLayout — `self` is the subobject at
// MCV+0x340; `sizeBounds` is a const SizeBounds& {width@0, has@4, height@8,
// has@0xc}. Records the active split's MCV (so a menu command knows what to
// grow) and tiles N panes once the split has grown past two.
using Calc_t = void*(__fastcall*)(void*, void*, void*);
inline Calc_t o_calc = nullptr;
inline void* __fastcall h_calc(void* self, void* out, void* sizeBounds) {
  void* mcv = edge::McvFromLayoutBase(self);
  g_activeMcv = mcv;
  const bool isBounded = edge::Field<uint8_t>(sizeBounds, 0x4) == 1 &&
                         edge::Field<uint8_t>(sizeBounds, 0xc) == 1;
  const int width = edge::Field<int32_t>(sizeBounds, 0x0);
  const int height = edge::Field<int32_t>(sizeBounds, 0x8);
  // Track the content size on every pass (native too) so a resize snaps.
  const bool isResized = isBounded && anim::NoteHost(mcv, width, height);
  const bool isNativeLayout =
      !SplitActive() || edge::McvCount(mcv) <= 2 || !isBounded;
  if (isNativeLayout) {
    return o_calc(self, out, sizeBounds);
  }
  // The MCV was grown past two containers. Tile exactly as many as the live split
  // has members (1 once it has been collapsed); surplus containers are hidden.
  const int paneCount = edge::McvActiveSplitPanes(mcv);
  return geometry::TilePanes(mcv, out, width, height, paneCount, isResized);
}

// MultiContentsView::SetActiveIndex — Edge CHECKs index < 2 (int3 when a pane
// 3+ takes focus) and only clears the primary flag on "the other" of panes 0/1.
// For a grown split, do the same work for N panes: record the index, flag that
// pane primary and every other pane not, then refresh the borders. A stock split
// runs the original.
using SetActive_t = void(__fastcall*)(void*, int);
inline SetActive_t o_set_active = nullptr;
inline void __fastcall h_set_active(void* mcv, int index) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  const bool isStockSplit = count <= 2;
  if (isStockSplit) {
    return o_set_active(mcv, index);
  }
  const bool isVisiblePane =
      index >= 0 && index < count &&
      edge::ViewGetVisible(edge::McvContainerAt(mcv, index));
  if (!isVisiblePane) {
    // Edge would CHECK-crash on it.
    Log("[active] ignored SetActiveIndex(%d) of %d", index, count);
    return;
  }
  edge::Field<int32_t>(mcv, off::MCV_active_index) = index;
  for (int i = 0; i < count; i++) {
    edge::Field<uint8_t>(edge::ContainerWebView(edge::McvContainerAt(mcv, i)),
                         off::CWV_primary_flag) = i == index ? 1 : 0;
  }
  edge::McvUpdateBorders(mcv);
  Log("[active] pane %d of %d", index, count);
}

// MultiContentsView::GetFocusableViewsMapFor: Edge keeps a focus map only for
// the two containers it built, so for pane 3+ the lookup is null, and
// switching to a tab whose pane is ours crashed in the caller. Our panes get
// an empty map instead: no stored focus to restore there.
using FocusMap_t = void*(__fastcall*)(void*, void*);
inline FocusMap_t o_focus_map = nullptr;
inline void* __fastcall h_focus_map(void* mcv, void* container) {
  void* focusMap = o_focus_map(mcv, container);
  if (focusMap) {
    return focusMap;
  }
  alignas(8) static uint8_t emptyMap[off::FocusableViewsMap_size] = {};
  return emptyMap;
}

// split_tabs::LogSplitView{Created,Updated}UKM: Edge's split metrics CHECK that
// a split has exactly two tabs, so a grown split crashed Edge on them (seen: a
// pane of three navigating). A metric is not worth a crash: any other split
// skips them.
using SplitUkm_t = void(__fastcall*)(void*, void*);
inline SplitUkm_t o_split_created_ukm = nullptr;
inline SplitUkm_t o_split_updated_ukm = nullptr;
inline bool IsTwoTabSplit(void* tsm, void* splitId) {
  return edge::SplitMembers(tsm, splitId).Count() == 2;
}
inline void __fastcall h_split_created_ukm(void* tsm, void* splitId) {
  if (IsTwoTabSplit(tsm, splitId)) {
    o_split_created_ukm(tsm, splitId);
  }
}
inline void __fastcall h_split_updated_ukm(void* tsm, void* splitId) {
  if (IsTwoTabSplit(tsm, splitId)) {
    o_split_updated_ukm(tsm, splitId);
  }
}

// views::Button's mouse handlers, shared by every button in Edge. A grip has no
// handler of its own (grip.h): its press starts a pane drag and is claimed, so
// views keeps sending that button the drags and the release with its capture.
// Any other button runs the original untouched.
using ButtonPressed_t = bool(__fastcall*)(void*, const void*);
using ButtonReleased_t = void(__fastcall*)(void*, const void*);
using ButtonCaptureLost_t = void(__fastcall*)(void*);
inline ButtonPressed_t o_pressed = nullptr;
inline ButtonPressed_t o_dragged = nullptr;
inline ButtonReleased_t o_released = nullptr;
inline ButtonCaptureLost_t o_capture_lost = nullptr;

// views::EdgeTwoDimensionalResizeArea's mouse handlers: Edge's own divider
// class, shared by its two-pane handle and our per-seam dividers (dividers.h).
// A press on one of ours starts a seam drag and is claimed, so views keeps
// sending it the drags and the release; Edge's own handle runs the original.
using DividerEvent_t = bool(__fastcall*)(void*, const void*);
using DividerReleased_t = void(__fastcall*)(void*, const void*);
using DividerCaptureLost_t = void(__fastcall*)(void*);
inline DividerEvent_t o_divider_pressed = nullptr;
inline DividerEvent_t o_divider_dragged = nullptr;
inline DividerReleased_t o_divider_released = nullptr;
inline DividerCaptureLost_t o_divider_capture_lost = nullptr;

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
  // The second click of a double click resets that split to equal sizes, as
  // Edge's own handle does for its two panes.
  const bool isDoubleClick = edge::MouseClickCount(event) == 2;
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
    drag::Move(edge::EventRootLocation(event));
  }
  return o_dragged(button, event);
}

inline void __fastcall h_released(void* button, const void* event) {
  if (grip::IsGrip(button)) {
    drag::Release();
  }
  // A ⋯ d-pad arrow released over itself: a click. Run it once this event is
  // done (closing the menu destroys the arrow).
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
// split's last tab and dissolves the split. In a split of three or more the X
// closes exactly its own pane instead (split_ops::ClosePane). Every other click,
// and a two-pane split, runs the original.
using ButtonClick_t = void(__fastcall*)(void*, void*);
inline ButtonClick_t o_button_click = nullptr;
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

// ThemeService::RemoveUnusedThemes: Chromium's one-active-theme garbage
// collector. While theming is active we keep both the light and the dark theme
// installed so a mode flip switches between them instantly, so the GC must not
// run — it would unload whichever theme is not currently active. Suppressed only
// while the feature is on; otherwise Edge GCs normally. This hook is NOT a split
// hook: it stays enabled whenever theming is on, even with the split switched off
// (control.h re-enables it after disabling the split hooks).
using RemoveUnusedThemes_t = void(__fastcall*)(void*);
inline RemoveUnusedThemes_t o_remove_unused_themes = nullptr;
inline void __fastcall h_remove_unused_themes(void* themeService) {
  if (theme::g_isActive) {
    return;
  }
  o_remove_unused_themes(themeService);
}

template <class T>
inline bool Install(uintptr_t rva, void* detour, T** original,
                    const char* name) {
  const bool isHooked =
      MH_CreateHook(edge::At(rva), detour,
                    reinterpret_cast<void**>(original)) == MH_OK &&
      MH_EnableHook(edge::At(rva)) == MH_OK;
  if (!isHooked) {
    Log("[err] hook %s failed", name);
    return false;
  }
  Log("[ok] hooked %s", name);
  return true;
}

inline bool InstallAll() {
  const bool isInitialized = MH_Initialize() == MH_OK;
  if (!isInitialized) {
    Log("[err] MH_Initialize failed");
    return false;
  }
  Install(rva::MultiContentsView_SetWebContentsAtIndex,
          reinterpret_cast<void*>(h_setwc), &o_setwc, "SetWebContentsAtIndex");
  Install(rva::MultiContentsView_CalculateProposedLayout,
          reinterpret_cast<void*>(h_calc), &o_calc, "CalculateProposedLayout");
  Install(rva::SplitTabs_LogSplitViewCreatedUKM,
          reinterpret_cast<void*>(h_split_created_ukm), &o_split_created_ukm,
          "split_tabs::LogSplitViewCreatedUKM");
  Install(rva::SplitTabs_LogSplitViewUpdatedUKM,
          reinterpret_cast<void*>(h_split_updated_ukm), &o_split_updated_ukm,
          "split_tabs::LogSplitViewUpdatedUKM");
  Install(rva::MultiContentsView_GetFocusableViewsMapFor,
          reinterpret_cast<void*>(h_focus_map), &o_focus_map,
          "MultiContentsView::GetFocusableViewsMapFor");
  Install(rva::MultiContentsView_SetActiveIndex,
          reinterpret_cast<void*>(h_set_active), &o_set_active,
          "SetActiveIndex");
  Install(rva::ResizeArea2D_OnMousePressed,
          reinterpret_cast<void*>(h_divider_pressed), &o_divider_pressed,
          "EdgeTwoDimensionalResizeArea::OnMousePressed");
  Install(rva::ResizeArea2D_OnMouseDragged,
          reinterpret_cast<void*>(h_divider_dragged), &o_divider_dragged,
          "EdgeTwoDimensionalResizeArea::OnMouseDragged");
  Install(rva::ResizeArea2D_OnMouseReleased,
          reinterpret_cast<void*>(h_divider_released), &o_divider_released,
          "EdgeTwoDimensionalResizeArea::OnMouseReleased");
  Install(rva::ResizeArea2D_OnMouseCaptureLost,
          reinterpret_cast<void*>(h_divider_capture_lost),
          &o_divider_capture_lost,
          "EdgeTwoDimensionalResizeArea::OnMouseCaptureLost");
  Install(rva::SplitWindowControlMenu_BuildMenuModel,
          reinterpret_cast<void*>(menu::h_build), &menu::o_build,
          "SplitWindowControlMenu::BuildMenuModel");
  Install(rva::BrowserWebContentsDelegate_ShouldOverrideNewWindowCreation,
          reinterpret_cast<void*>(links::h_should_override),
          &links::o_should_override,
          "BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation");
  Install(rva::BrowserWebContentsDelegate_AddNewContents,
          reinterpret_cast<void*>(links::h_add_new_contents),
          &links::o_add_new_contents,
          "BrowserWebContentsDelegate::AddNewContents");
  Install(rva::SplitWindowControllerV2_UpdateLinkedModeState,
          reinterpret_cast<void*>(links::h_update_linked_mode),
          &links::o_update_linked_mode,
          "SplitWindowControllerImplV2::UpdateLinkedModeState");
  Install(rva::SplitWindowLinkModeThrottle_MaybeCreate,
          reinterpret_cast<void*>(links::h_link_throttle),
          &links::o_link_throttle,
          "SplitWindowLinkModeNavigationThrottle::MaybeCreateThrottleFor");
  Install(rva::SplitWindowControllerV2_OpenTabInCurrentSplitWindow,
          reinterpret_cast<void*>(offer::h_open_tab), &offer::o_open_tab,
          "SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow");
  Install(rva::SplitWindowStartingUIHandler_GetTabs,
          reinterpret_cast<void*>(offer::h_get_tabs), &offer::o_get_tabs,
          "SplitWindowStartingUIHandler::GetTabs");
  Install(rva::SplitWindowController_IsSplitWindowWebContents,
          reinterpret_cast<void*>(offer::h_is_split_contents),
          &offer::o_is_split_contents,
          "SplitWindowController::IsSplitWindowWebContents");
  Install(rva::StartingPageLeftSplitTask_RunOnce,
          reinterpret_cast<void*>(offer::h_page_left_split),
          &offer::o_page_left_split,
          "OnSplitTabRemoved's starting page task (RunOnce)");
  Install(rva::SplitWindowStartingUIHandler_CloseSplitWindow,
          reinterpret_cast<void*>(offer::h_close_page), &offer::o_close_page,
          "SplitWindowStartingUIHandler::CloseSplitWindow");
  Install(rva::TabDragController_ContinueDragging,
          reinterpret_cast<void*>(tabdrag::h_continue_dragging),
          &tabdrag::o_continue_dragging, "TabDragController::ContinueDragging");
  Install(rva::TabDragController_EndDrag,
          reinterpret_cast<void*>(tabdrag::h_end_drag), &tabdrag::o_end_drag,
          "TabDragController::EndDrag");
  Install(rva::EventTracker_OnEvent,
          reinterpret_cast<void*>(tabdrag::h_tracker_event),
          &tabdrag::o_tracker_event, "EventTracker::OnEvent");
  Install(rva::MenuItemView_CalculateProposedLayout,
          reinterpret_cast<void*>(menu::h_row_layout), &menu::o_row_layout,
          "MenuItemView::CalculateProposedLayout");
  Install(rva::SplitWindowControlMenu_PopulateMenu,
          reinterpret_cast<void*>(menu::h_populate), &menu::o_populate,
          "SplitWindowControlMenu::PopulateMenu");
  Install(rva::SplitWindowControlMenu_ShouldExecuteWithoutClosing,
          reinterpret_cast<void*>(menu::h_should_execute),
          &menu::o_should_execute,
          "SplitWindowControlMenu::ShouldExecuteCommandWithoutClosingMenu");
  Install(rva::SplitWindowControlMenu_ExecuteCommand,
          reinterpret_cast<void*>(menu::h_control_exec), &menu::o_control_exec,
          "SplitWindowControlMenu::ExecuteCommand");
  Install(rva::ControlButtons_ToggleContextMenu,
          reinterpret_cast<void*>(menu::h_toggle), &menu::o_toggle,
          "ControlButtons::ToggleContextMenu");
  Install(rva::ControlButtons_HandleButtonClick,
          reinterpret_cast<void*>(h_button_click), &o_button_click,
          "ControlButtons::HandleButtonClick");
  Install(rva::Button_OnMousePressed, reinterpret_cast<void*>(h_pressed),
          &o_pressed, "Button::OnMousePressed");
  Install(rva::Button_OnMouseDragged, reinterpret_cast<void*>(h_dragged),
          &o_dragged, "Button::OnMouseDragged");
  Install(rva::Button_OnMouseReleased, reinterpret_cast<void*>(h_released),
          &o_released, "Button::OnMouseReleased");
  Install(rva::Button_OnMouseCaptureLost,
          reinterpret_cast<void*>(h_capture_lost), &o_capture_lost,
          "Button::OnMouseCaptureLost");
  Install(rva::SplitWindowController_DoAction,
          reinterpret_cast<void*>(collapse::h_do_action),
          &collapse::o_do_action, "DoActionToSplitWindow");
  Install(rva::SplitTabMenuModel_ctor, reinterpret_cast<void*>(menu::h_ctor),
          &menu::o_ctor, "SplitTabMenuModel::ctor");
  Install(rva::SplitTabMenuModel_ExecuteCommand,
          reinterpret_cast<void*>(menu::h_exec), &menu::o_exec,
          "SplitTabMenuModel::ExecuteCommand");
  Install(rva::WebUISplitTabsControl_HandleContextMenu,
          reinterpret_cast<void*>(menu::h_handle_ctx), &menu::o_handle_ctx,
          "WebUISplitTabsControl::HandleContextMenu");
  Install(rva::ThemeService_RemoveUnusedThemes,
          reinterpret_cast<void*>(h_remove_unused_themes),
          &o_remove_unused_themes, "ThemeService::RemoveUnusedThemes");
  // Asking to handle links: pass-through until the feature is switched on
  // (protocol_handlers.h).
  Install(rva::ContentSettingRPHBubbleModel_CommitChanges,
          reinterpret_cast<void*>(protocol_handlers::h_commit_changes),
          &protocol_handlers::o_commit_changes,
          "ContentSettingRPHBubbleModel::CommitChanges");
  Install(rva::RegisterProtocolHandlerPermissionRequest_PermissionDecided,
          reinterpret_cast<void*>(protocol_handlers::h_permission_decided),
          &protocol_handlers::o_permission_decided,
          "RegisterProtocolHandlerPermissionRequest::PermissionDecided");
  // Safety patches: turn Edge's 2-pane CHECKs into fallbacks. They stay for the
  // life of the process (never restored on unload): they point at nothing of
  // ours, and a split still grown past two when this DLL unloads (hot-reload or
  // toggle-off) would otherwise hit a CHECK as soon as Edge touches pane 3+.
  const uint8_t previousGuard =
      edge::PatchByte(rva::SetWebContentsAtIndex_guard_imm, kLiftedIndexCap);
  Log("[patch] SetWebContentsAtIndex guard %u -> 0xff", previousGuard);
  uint8_t previousBytes[kCheckJumpLength] = {};
  // The a11y label's int3 at index >= 2.
  edge::PatchNop(rva::SplitA11yLabel_paneidx_jne, kCheckJumpLength,
                 previousBytes);
  Log("[patch] a11y pane-index check NOPed (was 0x%02x 0x%02x)",
      previousBytes[0], previousBytes[1]);
  // SetActiveIndex's int3 at index >= 2.
  edge::PatchNop(rva::SetActiveIndex_index_jae, kCheckJumpLength,
                 previousBytes);
  Log("[patch] SetActiveIndex index check NOPed (was 0x%02x 0x%02x)",
      previousBytes[0], previousBytes[1]);
  // The tab list piles a split bigger than a pair onto one slot; give each of
  // its tabs a slot of its own (tabrows.h).
  Install(rva::TabStripLayoutHelper_CalculateIdealBounds,
          reinterpret_cast<void*>(tabrows::h_calculate_ideal_bounds),
          &tabrows::o_calculate_ideal_bounds,
          "TabStripLayoutHelper::CalculateIdealBounds");
  // The chat button's chevron: independent of the split, so it stays live even
  // when the split is toggled off.
  assistant::Install();
  // The enhanced find rides Edge's own find bar, whatever the split does.
  find::Install();
  // The WebUI pages' light or dark follows the theme, whatever the split does.
  theme_follow::Install();
  control::Install();  // the add-pane control channel (UI-thread trigger)
  return true;
}

inline void RemoveAll() {
  protocol_handlers::Remove();
  control::Remove();
  MH_DisableHook(MH_ALL_HOOKS);  // the safety patches stay (see InstallAll)
  MH_Uninitialize();
}

}  // namespace fixedge::hooks
