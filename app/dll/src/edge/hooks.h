// hooks.h — installs every detour on Edge and removes them all: each hook is
// tagged with the feature it serves, so a feature this build stood down gets
// none, and the split's safety patches lift Edge's 2-pane CHECKs. The
// detours live with what they serve: edge/detours/ for the split view, the
// panes' input, theming and colours, each feature's own header for the rest.
#pragma once
#include <cstdint>
#include "MinHook.h"
#include "chrome/menu/menu_border.h"
#include "chrome/pane/menu.h"
#include "chrome/tabs/live_preview.h"
#include "chrome/tabs/tab_font.h"
#include "chrome/tabs/tab_frame.h"
#include "chrome/tabs/tab_shape.h"
#include "chrome/tabs/tabdrag.h"
#include "chrome/tabs/tabrows.h"
#include "chrome/toolbar/omnibox_frame.h"
#include "core/log.h"
#include "edge/api/binding.h"
#include "edge/availability.h"
#include "edge/detours/colour_detours.h"
#include "edge/detours/pane_input_detours.h"
#include "edge/detours/split_view_detours.h"
#include "edge/detours/theme_detours.h"
#include "edge/offsets.h"
#include "find/find.h"
#include "layout/collapse.h"
#include "permissions/protocol_handlers.h"
#include "split/control.h"
#include "split/links.h"
#include "split/offer.h"
#include "theme/follow/theme_follow.h"
#include "theme/remix/remix_darkmode.h"

namespace edgeplus::hooks {

// Each 2-pane CHECK that InstallAll NOPs is a `jcc rel32` (0F 8x + a 4-byte
// offset; see offsets.h).
inline constexpr size_t kCheckJumpLength = 6;
using edge::kLiftedIndexCap;

// A hook belongs to the feature it serves (tools/re/New-SignatureTable.ps1
// reads the tag, so the hooked entry stands that feature down when it is
// missing); a feature this build stood down gets none of its hooks.
template <class T>
inline bool Install(edge155::signatures::Feature feature, uintptr_t rva,
                    void* detour, T** original, const char* name) {
  if (!availability::IsAvailable(feature)) {
    Log("[skip] hook %s: its feature stood down in this Edge build", name);
    return false;
  }
  const bool isPlaced = edge::At(rva) != nullptr;
  if (!isPlaced) {
    Log("[skip] hook %s: no place in this Edge build", name);
    return false;
  }
  const bool isHooked =
      MH_CreateHook(edge::At(rva), detour,
                    reinterpret_cast<void**>(original)) == MH_OK &&
      edge::EnableHookAt(rva) == MH_OK;
  if (!isHooked) {
    Log("[err] hook %s failed", name);
    return false;
  }
  Log("[ok] hooked %s", name);
  return true;
}

[[nodiscard]] inline bool InstallAll() {
  using edge155::signatures::Feature;
  const bool isInitialized = MH_Initialize() == MH_OK;
  if (!isInitialized) {
    Log("[err] MH_Initialize failed");
    return false;
  }
  Install(Feature::kSplit,
          edge155::rva::MultiContentsView_SetWebContentsAtIndex,
          reinterpret_cast<void*>(h_setwc), &o_setwc, "SetWebContentsAtIndex");
  Install(Feature::kSplit,
          edge155::rva::MultiContentsView_CalculateProposedLayout,
          reinterpret_cast<void*>(h_calc), &o_calc, "CalculateProposedLayout");
  Install(Feature::kSplit, edge155::rva::SplitTabs_LogSplitViewCreatedUKM,
          reinterpret_cast<void*>(h_split_created_ukm), &o_split_created_ukm,
          "split_tabs::LogSplitViewCreatedUKM");
  Install(Feature::kSplit, edge155::rva::SplitTabs_LogSplitViewUpdatedUKM,
          reinterpret_cast<void*>(h_split_updated_ukm), &o_split_updated_ukm,
          "split_tabs::LogSplitViewUpdatedUKM");
  Install(Feature::kSplit,
          edge155::rva::MultiContentsView_GetFocusableViewsMapFor,
          reinterpret_cast<void*>(h_focus_map), &o_focus_map,
          "MultiContentsView::GetFocusableViewsMapFor");
  Install(Feature::kSplit, edge155::rva::kChromeReloadInternal,
          reinterpret_cast<void*>(h_reload_internal), &o_reload_internal,
          "chrome::ReloadInternal");
  Install(Feature::kSplit, edge155::rva::MultiContentsView_SetActiveIndex,
          reinterpret_cast<void*>(h_set_active), &o_set_active,
          "SetActiveIndex");
  Install(Feature::kSplit, edge155::rva::MultiContentsView_dtor,
          reinterpret_cast<void*>(h_mcv_dtor), &o_mcv_dtor,
          "MultiContentsView::~MultiContentsView");
  Install(Feature::kSplit, edge155::rva::MultiContentsView_CloseSplitView,
          reinterpret_cast<void*>(h_close_split_view), &o_close_split_view,
          "MultiContentsView::CloseSplitView");
  Install(Feature::kSplit, edge155::rva::ResizeArea2D_OnMousePressed,
          reinterpret_cast<void*>(h_divider_pressed), &o_divider_pressed,
          "EdgeTwoDimensionalResizeArea::OnMousePressed");
  Install(Feature::kSplit, edge155::rva::ResizeArea2D_OnMouseDragged,
          reinterpret_cast<void*>(h_divider_dragged), &o_divider_dragged,
          "EdgeTwoDimensionalResizeArea::OnMouseDragged");
  Install(Feature::kSplit, edge155::rva::ResizeArea2D_OnMouseReleased,
          reinterpret_cast<void*>(h_divider_released), &o_divider_released,
          "EdgeTwoDimensionalResizeArea::OnMouseReleased");
  Install(Feature::kSplit, edge155::rva::ResizeArea2D_OnMouseCaptureLost,
          reinterpret_cast<void*>(h_divider_capture_lost),
          &o_divider_capture_lost,
          "EdgeTwoDimensionalResizeArea::OnMouseCaptureLost");
  Install(Feature::kSplit, edge155::rva::SplitWindowControlMenu_BuildMenuModel,
          reinterpret_cast<void*>(menu::h_build), &menu::o_build,
          "SplitWindowControlMenu::BuildMenuModel");
  Install(
      Feature::kSplit,
      edge155::rva::BrowserWebContentsDelegate_ShouldOverrideNewWindowCreation,
      reinterpret_cast<void*>(links::h_should_override),
      &links::o_should_override,
      "BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation");
  Install(
      Feature::kSplit, edge155::rva::BrowserWebContentsDelegate_AddNewContents,
      reinterpret_cast<void*>(links::h_add_new_contents),
      &links::o_add_new_contents, "BrowserWebContentsDelegate::AddNewContents");
  Install(Feature::kSplit,
          edge155::rva::SplitWindowControllerV2_UpdateLinkedModeState,
          reinterpret_cast<void*>(links::h_update_linked_mode),
          &links::o_update_linked_mode,
          "SplitWindowControllerImplV2::UpdateLinkedModeState");
  Install(
      Feature::kSplit, edge155::rva::SplitWindowLinkModeThrottle_MaybeCreate,
      reinterpret_cast<void*>(links::h_link_throttle), &links::o_link_throttle,
      "SplitWindowLinkModeNavigationThrottle::MaybeCreateThrottleFor");
  Install(Feature::kSplit,
          edge155::rva::SplitWindowControllerV2_OpenTabInCurrentSplitWindow,
          reinterpret_cast<void*>(offer::h_open_tab), &offer::o_open_tab,
          "SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow");
  Install(Feature::kSplit, edge155::rva::SplitWindowStartingUIHandler_GetTabs,
          reinterpret_cast<void*>(offer::h_get_tabs), &offer::o_get_tabs,
          "SplitWindowStartingUIHandler::GetTabs");
  Install(Feature::kSplit,
          edge155::rva::SplitWindowController_IsSplitWindowWebContents,
          reinterpret_cast<void*>(offer::h_is_split_contents),
          &offer::o_is_split_contents,
          "SplitWindowController::IsSplitWindowWebContents");
  Install(Feature::kSplit, edge155::rva::StartingPageLeftSplitTask_RunOnce,
          reinterpret_cast<void*>(offer::h_page_left_split),
          &offer::o_page_left_split,
          "OnSplitTabRemoved's starting page task (RunOnce)");
  Install(Feature::kSplit,
          edge155::rva::SplitWindowStartingUIHandler_CloseSplitWindow,
          reinterpret_cast<void*>(offer::h_close_page), &offer::o_close_page,
          "SplitWindowStartingUIHandler::CloseSplitWindow");
  Install(Feature::kCyberpunkSkin, edge155::rva::EdgeTabStyleViews_GetPath,
          reinterpret_cast<void*>(tab_shape::h_get_path),
          &tab_shape::o_get_path, "EdgeTabStyleViews::GetPath");
  Install(Feature::kCyberpunkSkin, edge155::rva::SkPath_arcTo,
          reinterpret_cast<void*>(tab_shape::h_arc_to), &tab_shape::o_arc_to,
          "SkPath::arcTo");
  Install(Feature::kCyberpunkSkin,
          edge155::rva::EdgeTabStyleViews_IsLeftSplitTab,
          reinterpret_cast<void*>(tab_shape::h_is_left_split_tab),
          &tab_shape::o_is_left_split_tab, "EdgeTabStyleViews::IsLeftSplitTab");
  Install(Feature::kCyberpunkSkin, edge155::rva::SkRRect_setRectXY,
          reinterpret_cast<void*>(tab_shape::h_set_rect_xy),
          &tab_shape::o_set_rect_xy, "SkRRect::setRectXY");
  Install(Feature::kCyberpunkSkin, edge155::rva::Canvas_DrawRoundRect,
          reinterpret_cast<void*>(tab_shape::h_draw_round_rect),
          &tab_shape::o_draw_round_rect, "gfx::Canvas::DrawRoundRect");
  Install(Feature::kCyberpunkSkin,
          edge155::rva::EdgeTabStyleViews_IsRightSplitTab,
          reinterpret_cast<void*>(tab_shape::h_is_right_split_tab),
          &tab_shape::o_is_right_split_tab,
          "EdgeTabStyleViews::IsRightSplitTab (drop shadow)");
  Install(Feature::kCyberpunkSkin,
          edge155::rva::EdgeTabStyleViews_PaintBackgroundStroke,
          reinterpret_cast<void*>(tab_frame::h_paint_background_stroke),
          &tab_frame::o_paint_background_stroke,
          "EdgeTabStyleViews::PaintBackgroundStroke (tab frame)");
  Install(Feature::kCyberpunkSkin, edge155::rva::TabStrip_GetStrokeThickness,
          reinterpret_cast<void*>(tab_frame::h_get_stroke_thickness),
          &tab_frame::o_get_stroke_thickness,
          "TabStrip::GetStrokeThickness (tab frame)");
  Install(Feature::kCyberpunkSkin, edge155::rva::EdgeTabStyleViews_PaintTab,
          reinterpret_cast<void*>(tab_font::h_paint_tab),
          &tab_font::o_paint_tab, "EdgeTabStyleViews::PaintTab (title font)");
  Install(Feature::kCyberpunkSkin,
          edge155::rva::LocationBarView_EdgeCreateRoundRectBackground,
          reinterpret_cast<void*>(omnibox_frame::h_create_background),
          &omnibox_frame::o_create_background,
          "LocationBarView::EdgeCreateRoundRectBackground (omnibox frame)");
  Install(Feature::kCyberpunkSkin, edge155::rva::BubbleBorder_GetInsets,
          reinterpret_cast<void*>(menu_border::h_get_insets),
          &menu_border::o_get_insets,
          "BubbleBorder::GetInsets (menu border's top edge)");
  Install(Feature::kSplit, edge155::rva::TabDragController_ContinueDragging,
          reinterpret_cast<void*>(tabdrag::h_continue_dragging),
          &tabdrag::o_continue_dragging, "TabDragController::ContinueDragging");
  Install(Feature::kSplit, edge155::rva::TabDragController_EndDrag,
          reinterpret_cast<void*>(tabdrag::h_end_drag), &tabdrag::o_end_drag,
          "TabDragController::EndDrag");
  Install(Feature::kSplit, edge155::rva::EventTracker_OnEvent,
          reinterpret_cast<void*>(tabdrag::h_tracker_event),
          &tabdrag::o_tracker_event, "EventTracker::OnEvent");
  Install(Feature::kSplit, edge155::rva::MenuItemView_CalculateProposedLayout,
          reinterpret_cast<void*>(menu::h_row_layout), &menu::o_row_layout,
          "MenuItemView::CalculateProposedLayout");
  Install(Feature::kSplit, edge155::rva::SplitWindowControlMenu_PopulateMenu,
          reinterpret_cast<void*>(menu::h_populate), &menu::o_populate,
          "SplitWindowControlMenu::PopulateMenu");
  Install(Feature::kSplit,
          edge155::rva::SplitWindowControlMenu_ShouldExecuteWithoutClosing,
          reinterpret_cast<void*>(menu::h_should_execute),
          &menu::o_should_execute,
          "SplitWindowControlMenu::ShouldExecuteCommandWithoutClosingMenu");
  Install(Feature::kSplit, edge155::rva::SplitWindowControlMenu_ExecuteCommand,
          reinterpret_cast<void*>(menu::h_control_exec), &menu::o_control_exec,
          "SplitWindowControlMenu::ExecuteCommand");
  Install(Feature::kSplit, edge155::rva::ControlButtons_ToggleContextMenu,
          reinterpret_cast<void*>(menu::h_toggle), &menu::o_toggle,
          "ControlButtons::ToggleContextMenu");
  Install(Feature::kSplit, edge155::rva::ControlButtons_HandleButtonClick,
          reinterpret_cast<void*>(h_button_click), &o_button_click,
          "ControlButtons::HandleButtonClick");
  Install(Feature::kSplit, edge155::rva::Button_OnMousePressed,
          reinterpret_cast<void*>(h_pressed), &o_pressed,
          "Button::OnMousePressed");
  Install(Feature::kSplit, edge155::rva::Button_OnMouseDragged,
          reinterpret_cast<void*>(h_dragged), &o_dragged,
          "Button::OnMouseDragged");
  Install(Feature::kSplit, edge155::rva::Button_OnMouseReleased,
          reinterpret_cast<void*>(h_released), &o_released,
          "Button::OnMouseReleased");
  Install(Feature::kSplit, edge155::rva::Button_OnMouseCaptureLost,
          reinterpret_cast<void*>(h_capture_lost), &o_capture_lost,
          "Button::OnMouseCaptureLost");
  Install(Feature::kSplit, edge155::rva::SplitWindowController_DoAction,
          reinterpret_cast<void*>(collapse::h_do_action),
          &collapse::o_do_action, "DoActionToSplitWindow");
  Install(Feature::kSplit, edge155::rva::SplitTabMenuModel_ctor,
          reinterpret_cast<void*>(menu::h_ctor), &menu::o_ctor,
          "SplitTabMenuModel::ctor");
  Install(Feature::kSplit, edge155::rva::SplitTabMenuModel_ExecuteCommand,
          reinterpret_cast<void*>(menu::h_exec), &menu::o_exec,
          "SplitTabMenuModel::ExecuteCommand");
  Install(Feature::kSplit,
          edge155::rva::WebUISplitTabsControl_HandleContextMenu,
          reinterpret_cast<void*>(menu::h_handle_ctx), &menu::o_handle_ctx,
          "WebUISplitTabsControl::HandleContextMenu");
  Install(Feature::kTheming, edge155::rva::ThemeService_RemoveUnusedThemes,
          reinterpret_cast<void*>(h_remove_unused_themes),
          &o_remove_unused_themes, "ThemeService::RemoveUnusedThemes");
  Install(Feature::kTheming,
          edge155::rva::ThemeService_OnThemeBuiltFromExtension,
          reinterpret_cast<void*>(h_on_theme_built), &o_on_theme_built,
          "ThemeService::OnThemeBuiltFromExtension (no bubble for Edge+)");
  Install(Feature::kTheming, edge155::rva::ThemeService_UseDefaultTheme,
          reinterpret_cast<void*>(h_use_default_theme), &o_use_default_theme,
          "ThemeService::UseDefaultTheme (the user's return to default)");
  Install(Feature::kTheming, edge155::rva::BrowserThemePack_GetColor,
          reinterpret_cast<void*>(h_getcolor), &o_getcolor,
          "BrowserThemePack::GetColor (vertical-tab recolour)");
  Install(Feature::kCyberpunkSkin, edge155::rva::ColorProvider_GetColor,
          reinterpret_cast<void*>(h_getuicolor), &o_getuicolor,
          "ui::ColorProvider::GetColor (selected-tab ink)");
  Install(Feature::kTheming, edge155::rva::EdgeTab_UpdateForegroundColors,
          reinterpret_cast<void*>(h_update_foreground), &o_update_foreground,
          "EdgeTab::UpdateForegroundColors (selected-tab ink)");
  Install(Feature::kProtocolHandlers,
          edge155::rva::ContentSettingRPHBubbleModel_CommitChanges,
          reinterpret_cast<void*>(protocol_handlers::h_commit_changes),
          &protocol_handlers::o_commit_changes,
          "ContentSettingRPHBubbleModel::CommitChanges");
  Install(
      Feature::kProtocolHandlers,
      edge155::rva::RegisterProtocolHandlerPermissionRequest_PermissionDecided,
      reinterpret_cast<void*>(protocol_handlers::h_permission_decided),
      &protocol_handlers::o_permission_decided,
      "RegisterProtocolHandlerPermissionRequest::PermissionDecided");
  // Safety patches on Edge's 2-pane CHECKs. Never restored on unload: they point
  // at nothing of ours, and a split still grown past two when this DLL unloads
  // would otherwise hit a CHECK as soon as Edge touches pane 3+.
  const bool isSplitAvailable = availability::IsAvailable(Feature::kSplit);
  if (isSplitAvailable) {
    const uint8_t previousGuard = edge::PatchByte(
        edge155::rva::SetWebContentsAtIndex_guard_imm, kLiftedIndexCap);
    Log("[patch] SetWebContentsAtIndex guard %u -> 0x%x", previousGuard,
        kLiftedIndexCap);
    uint8_t previousBytes[kCheckJumpLength] = {};
    edge::PatchNop(edge155::rva::SplitA11yLabel_paneidx_jne, kCheckJumpLength,
                   previousBytes);
    Log("[patch] a11y pane-index check NOPed (was 0x%02x 0x%02x)",
        previousBytes[0], previousBytes[1]);
    edge::PatchNop(edge155::rva::SetActiveIndex_index_jae, kCheckJumpLength,
                   previousBytes);
    Log("[patch] SetActiveIndex index check NOPed (was 0x%02x 0x%02x)",
        previousBytes[0], previousBytes[1]);
  }
  Install(Feature::kSplit,
          edge155::rva::TabStripLayoutHelper_CalculateIdealBounds,
          reinterpret_cast<void*>(tabrows::h_calculate_ideal_bounds),
          &tabrows::o_calculate_ideal_bounds,
          "TabStripLayoutHelper::CalculateIdealBounds");
  Install(Feature::kSplit,
          edge155::rva::EdgeTabDragContextImpl_CalculateBoundsForDraggedViews,
          reinterpret_cast<void*>(tabrows::h_dragged_bounds),
          &tabrows::o_dragged_bounds,
          "EdgeTabDragContextImpl::CalculateBoundsForDraggedViews");
  Install(Feature::kSplit, edge155::rva::Tab_IsActive,
          reinterpret_cast<void*>(tabrows::h_is_active), &tabrows::o_is_active,
          "Tab::IsActive (detach preview)");
  // A feature with an address this Edge build could not place stays out
  // entirely (resolve.h); the hooks above skip theirs one by one.
  if (availability::IsAvailable(Feature::kEnhancedFind)) {
    find::Install();
  }
  const bool isThemeFollowWanted =
      availability::IsAvailable(Feature::kWebUiFollowsTheme) ||
      availability::IsAvailable(Feature::kRemixTheme);
  if (isThemeFollowWanted) {
    theme_follow::Install();
  }
  if (availability::IsAvailable(Feature::kRemixTheme)) {
    remix_darkmode::Install();
  }
  if (availability::IsAvailable(Feature::kLivePreview)) {
    live_preview::Install();
  }
  control::Install();
  return true;
}

inline void RemoveAll() {
  // The UI-side teardown also turns the link handlers off, on the UI thread;
  // they are removed again in case it could not run.
  control::Remove();
  protocol_handlers::Remove();
  MH_DisableHook(MH_ALL_HOOKS);  // the safety patches stay (see InstallAll)
  MH_Uninitialize();
}

}  // namespace edgeplus::hooks
