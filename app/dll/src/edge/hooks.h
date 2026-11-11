// hooks.h — the detours on Edge's split methods and their install. Every detour
// is gated so a stock (<=2) split runs the original untouched.
#pragma once
#include <cstdint>
#include <cstring>
#include <utility>
#include "MinHook.h"
#include "edge/availability.h"
#include "edge/edge.h"
#include "theme/focus.h"
#include "theme/theme_tone.h"
#include "theme/cyberpunk_skin.h"
#include "theme/cyberpunk_palette.h"
#include "chrome/colours/colour_ids.h"
#include "chrome/colours/colour_dump.h"
#include "theme/skin_tone.h"
#include "theme/theme_hold.h"
#include "theme/theme_notice.h"
#include "core/wear_report.h"
#include "theme/theme.h"
#include "split/panes.h"
#include "layout/geometry.h"
#include "layout/collapse.h"
#include "chrome/pane/menu.h"
#include "split/control.h"
#include "theme/protocol_handlers.h"
#include "chrome/pane/dividers.h"
#include "chrome/tabs/tabrows.h"
#include "chrome/tabs/tab_shape.h"
#include "chrome/tabs/tab_frame.h"
#include "chrome/tabs/tab_font.h"
#include "chrome/toolbar/omnibox_frame.h"
#include "chrome/menu/menu_border.h"
#include "chrome/pane/drag.h"
#include "chrome/pane/grip.h"
#include "layout/state.h"
#include "find/find.h"
#include "theme/theme_follow.h"
#include "chrome/tabs/live_preview.h"

namespace edgeplus::hooks {

// Each 2-pane CHECK that InstallAll NOPs is a `jcc rel32` (0F 8x + a 4-byte
// offset; see offsets.h).
inline constexpr size_t kCheckJumpLength = 6;
using edge::kLiftedIndexCap;
inline constexpr int kDoubleClickCount = 2;

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. Its index<2
// guard is lifted at install (the safety patches in InstallAll).
using SetWc_t = void*(__fastcall*)(void*, void*, uint32_t);
inline constinit SetWc_t o_setwc = nullptr;
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
  // Edge only force-paints the panes it natively tracks, and only watches the
  // first and last member for hover; a fed background member stays blank until
  // shown.
  if (webContents) {
    edge::WebContentsWasShown(webContents);
    edge::ListenSplitInput(webContents);
  }
  panes::EnsureControlButtons(mcv, static_cast<int>(index));
  grip::EnsureGrips(mcv);
  dividers::Ensure(mcv);
  return result;
}

// LayoutManagerBase::CalculateProposedLayout — `self` is the subobject at
// MCV+0x340; `sizeBounds` is a const SizeBounds& (offsets.h SIZEBOUNDS_*).
using Calc_t = void*(__fastcall*)(void*, void*, void*);
inline constinit Calc_t o_calc = nullptr;
inline void* __fastcall h_calc(void* self, void* out, void* sizeBounds) {
  void* mcv = edge::McvFromLayoutBase(self);
  g_activeMcv = mcv;
  // A split torn off into another window leaves without CloseSplitView: the
  // old window's grown panes kept its pages, now shown in the new window. A
  // page under two windows' accessibility trees is the suspected cause of a
  // hang seen in Edge's AXPosition walks once the old window was focused.
  // With no split shown, no grown pane holds a page.
  const bool isNoSplitShown =
      edge::McvActiveSplitPanes(mcv) < split_data::kMinSplitMembers;
  if (isNoSplitShown) {
    panes::EmptyGrownContainers(mcv);
  }
  tabdrag::OnLaidOut(mcv);
  const bool isBounded =
      edge::Field<uint8_t>(sizeBounds, edge155::off::SIZEBOUNDS_has_width) ==
          1 &&
      edge::Field<uint8_t>(sizeBounds, edge155::off::SIZEBOUNDS_has_height) ==
          1;
  const int width =
      edge::Field<int32_t>(sizeBounds, edge155::off::SIZEBOUNDS_width);
  const int height =
      edge::Field<int32_t>(sizeBounds, edge155::off::SIZEBOUNDS_height);
  // On every pass, native too, so a resize snaps.
  const bool isResized = isBounded && anim::NoteHost(mcv, width, height);
  const bool isNativeLayout =
      !SplitActive() ||
      edge::McvCount(mcv) <=
          static_cast<size_t>(dividers::kStockContainerCount) ||
      !isBounded;
  if (isNativeLayout) {
    return o_calc(self, out, sizeBounds);
  }
  // 1 once collapsed; surplus containers are hidden.
  const int paneCount = edge::McvActiveSplitPanes(mcv);
  return geometry::TilePanes(mcv, out, width, height, paneCount, isResized);
}

// MultiContentsView::SetActiveIndex — Edge CHECKs index < 2 (int3 when a pane
// 3+ takes focus) and only clears the primary flag on "the other" of panes 0/1,
// so a grown split does the same work for N panes.
using SetActive_t = void(__fastcall*)(void*, int);
inline constinit SetActive_t o_set_active = nullptr;
inline void __fastcall h_set_active(void* mcv, int index) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  const bool isStockSplit = count <= dividers::kStockContainerCount;
  if (isStockSplit) {
    return o_set_active(mcv, index);
  }
  const bool isVisiblePane =
      index >= 0 && index < count &&
      edge::ViewGetVisible(edge::McvContainerAt(mcv, index));
  if (!isVisiblePane) {
    Log("[active] ignored SetActiveIndex(%d) of %d", index, count);
    return;
  }
  edge::Field<int32_t>(mcv, edge155::off::MCV_active_index) = index;
  for (int i = 0; i < count; i++) {
    edge::Field<uint8_t>(edge::ContainerWebView(edge::McvContainerAt(mcv, i)),
                         edge155::off::CWV_primary_flag) = i == index ? 1 : 0;
  }
  edge::McvUpdateBorders(mcv);
  Log("[active] pane %d of %d", index, count);
}

// MultiContentsView::~MultiContentsView: every pointer we keep to the view goes
// first. A flight still animating it relaid out the destroyed view on the next
// tick (seen: tearing a split off into its own window crashed Edge).
using McvDtor_t = void(__fastcall*)(void*);
inline constinit McvDtor_t o_mcv_dtor = nullptr;
inline void __fastcall h_mcv_dtor(void* mcv) {
  anim::ForgetSplitView(mcv);
  dividers::ForgetSplitView(mcv);
  grip::ForgetSplitView(mcv);
  drag::ForgetSplitView(mcv);
  tabdrag::ForgetSplitView(mcv);
  ForgetSplitView(mcv);
  Log("[mcv] a split view is going: forgot it");
  o_mcv_dtor(mcv);
}

// MultiContentsView::CloseSplitView: Edge empties pane 1 as the split closes;
// our panes are emptied with it. Every way a split shrinks or goes (a pane
// closed or dragged out, the split dissolved, another tab shown) comes through
// here, since Edge only ever removes a member by dissolving the split.
using CloseSplitView_t = void(__fastcall*)(void*);
inline constinit CloseSplitView_t o_close_split_view = nullptr;
inline void __fastcall h_close_split_view(void* mcv) {
  o_close_split_view(mcv);
  panes::EmptyGrownContainers(mcv);
}

// chrome::ReloadInternal(browser, disposition, bypassCache): reloads every
// selected tab once more are selected than the active tab's split needs, and a
// split of three or more has all its members selected, so the reload button
// reloaded every pane. In such a split the active pane alone is reloaded.
using ReloadInternal_t = void(__fastcall*)(void*, int32_t, bool);
inline constinit ReloadInternal_t o_reload_internal = nullptr;
inline void __fastcall h_reload_internal(void* browser, int32_t disposition,
                                         bool isBypassingCache) {
  void* const tsm = edge::TabStripModelOf(browser);
  void* const activeTab = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool isActiveTabInSplit = activeTab && edge::TabIsSplit(activeTab);
  edge::OptionalSplitId splitId = {};
  int paneCount = 0;
  if (isActiveTabInSplit) {
    splitId = edge::TabSplitId(activeTab);
    paneCount =
        splitId.hasValue ? edge::SplitMembers(tsm, splitId.token).Count() : 0;
  }
  const bool isGrownSplit = paneCount > split_data::kMinSplitMembers;
  const bool isInCurrentTab =
      disposition == edge155::off::DISPOSITION_current_tab;
  const bool isReloadingOnlyActivePane = isGrownSplit && isInCurrentTab;
  if (!isReloadingOnlyActivePane) {
    return o_reload_internal(browser, disposition, isBypassingCache);
  }
  edge::ReloadPage(edge::TabContents(activeTab),
                   isBypassingCache ? edge155::off::RELOADTYPE_bypassing_cache
                                    : edge155::off::RELOADTYPE_normal);
  Log("[reload] active pane of a %d-pane split only", paneCount);
}

// MultiContentsView::GetFocusableViewsMapFor: Edge keeps a focus map only for
// the two containers it built, so for pane 3+ the lookup is null, and
// switching to a tab whose pane is ours crashed in the caller. Our panes get
// an empty map instead: no stored focus to restore there.
using FocusMap_t = void*(__fastcall*)(void*, void*);
inline constinit FocusMap_t o_focus_map = nullptr;
inline void* __fastcall h_focus_map(void* mcv, void* container) {
  void* focusMap = o_focus_map(mcv, container);
  if (focusMap) {
    return focusMap;
  }
  alignas(8) static uint8_t emptyMap[edge155::off::FocusableViewsMap_size] = {};
  return emptyMap;
}

// split_tabs::LogSplitView{Created,Updated}UKM: Edge's split metrics CHECK that
// a split has exactly two tabs, so a grown split crashed Edge on them (seen: a
// pane of three navigating).
using SplitUkm_t = void(__fastcall*)(void*, void*);
inline constinit SplitUkm_t o_split_created_ukm = nullptr;
inline constinit SplitUkm_t o_split_updated_ukm = nullptr;
inline bool IsTwoTabSplit(void* tsm, void* splitId) {
  return edge::SplitMembers(tsm, splitId).Count() == tabrows::kPairedMembers;
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

// ThemeService::RemoveUnusedThemes: Chromium's one-active-theme garbage
// collector. Theming keeps both the light and the dark theme installed so a mode
// flip is instant, and the GC would unload whichever is not active. Not a split
// hook: control.h keeps it enabled with the split off.
using RemoveUnusedThemes_t = void(__fastcall*)(void*);
inline constinit RemoveUnusedThemes_t o_remove_unused_themes = nullptr;
inline void __fastcall h_remove_unused_themes(void* themeService) {
  if (theme::g_isActive) {
    return;
  }
  o_remove_unused_themes(themeService);
}

// ThemeService::OnThemeBuiltFromExtension(this, id, pack, suppress_infobar):
// asks for no "Installed theme ... Undo" bubble when the theme is one Edge+
// applies itself (theme_notice.h). A store install is any other id and passes
// through untouched, bubble and all. Rides theming, not the split (control.h).
using OnThemeBuilt_t = void(__fastcall*)(void*, const void*, void*, bool);
inline constinit OnThemeBuilt_t o_on_theme_built = nullptr;
inline void __fastcall h_on_theme_built(void* themeService, const void* themeId,
                                        void* pack, bool isSuppressed) {
  const std::string idText = edge::StdStringText(themeId);
  const theme_notice::Assigned assigned{
      std::string_view(theme::g_lightTheme.value, theme::g_lightTheme.length),
      std::string_view(theme::g_darkTheme.value, theme::g_darkTheme.length),
      theme::g_isActive};
  const bool isKeptOff = theme_hold::ShouldKeepOff(
      std::string_view(theme::g_heldTheme.value, theme::g_heldTheme.length),
      idText, theme::g_ownSwitch.IsSettling(GetTickCount64()));
  if (isKeptOff) {
    // Not handed on, so the theme Edge just installed is never swapped in. Edge
    // has already recorded it as the current one, so what the mode wants is put
    // back (the apply runs on the UI thread, from the channel).
    Log("[theme] kept %s installed but off (added without Shift)",
        idText.c_str());
    channel::Post(channel::CodeOf(channel::Command::kApplyTheme));
    return;
  }
  const bool isSuppressedNow =
      theme_notice::ShouldSuppress(assigned, idText, isSuppressed);
  const bool isSuppressedByUs = isSuppressedNow && !isSuppressed;
  Log("[theme] built %s: bubble %s", idText.c_str(),
      isSuppressedByUs ? "suppressed (Edge+ theme)" : "left to Edge");
  o_on_theme_built(themeService, themeId, pack, isSuppressedNow);
  theme::g_ownSwitch.Built(idText);
  wear_report::Report(idText, theme_wear::CauseOfTheme(assigned, idText));
}

// ThemeService::UseDefaultTheme(this at its +0x10 base, ...): every return to
// Edge's default theme, Edge's own "Restore default" included, ends here. The
// second register is passed through untouched (its meaning is not needed).
// Reported with who caused it (theme_wear.h), so the panel can follow the
// user's choice; the skin's colours from the last theme pack go stale either way.
using UseDefaultTheme_t = void(__fastcall*)(void*, uintptr_t);
inline constinit UseDefaultTheme_t o_use_default_theme = nullptr;
inline void __fastcall h_use_default_theme(void* themeServiceBase,
                                           uintptr_t argument) {
  o_use_default_theme(themeServiceBase, argument);
  theme::g_isDefaultThemeApplied = true;
  const theme_wear::Cause cause = theme_wear::CauseOfDefault(
      theme::g_isUsingDefaultOurselves,
      theme::g_ownSwitch.IsSettling(GetTickCount64()));
  Log("[theme] default theme worn (%s)",
      cause == theme_wear::Cause::kUser ? "the user's choice" : "Edge+");
  wear_report::Report({}, cause);
}

// BrowserThemePack::GetColor(this, id, SkColor* out): a theme extension's colour
// for `id`, false when the theme left it unset. Edge derives the whole vertical-
// tab strip from the toolbar colour (offsets.h VTABCOLOR_*), so a theme cannot
// give the selected tab a colour of its own; the Cyberpunk skin gives it none,
// so the rail shows through its frame (tab_frame.h), and picks the idle titles'
// and the hover's colours from the theme's own (SkinColorsOf). With the skin
// off every colour is Edge's. Reads go through o_getcolor, never the hook, so
// it cannot recurse.
// The colour mixer reads this id when DoSetTheme (theme.h) rebuilds the
// vertical-tab ColorProvider, so applying the theme repaints the rail at once;
// control.h re-applies it when the skin is switched. Rides the skin, which
// needs theming, not the split (control.h).
// Set from the theme each time Edge rebuilds the colours.
inline constinit theme_tone::OptionalColor g_idleLabel{};
// The selected and the hovered tab both read the selected-fill id; only the
// caller tells them apart. Edge paints a hovered tab as the active one, at the
// hover's opacity, on a layer over its inactive fill (EdgeTabStyleViews::PaintTab),
// and a custom theme's selected fill stands for every tab state, so the hover
// came out the full selected colour. The return addresses of that hover-layer
// paint, of the inactive fill (PaintInactiveTabBackground) and of the hover
// target (CalculateTargetColors) on the stack of the pack read mark a hover read.
inline constexpr ULONG kPaintStackDepth = 8;
inline bool IsReadForHover() {
  void* frames[kPaintStackDepth] = {};
  const USHORT count =
      CaptureStackBackTrace(0, kPaintStackDepth, frames, nullptr);
  const void* hoverCallers[] = {
      edge::At(edge155::rva::EdgeTabStyleViews_PaintTab_hover_layer_return),
      edge::At(
          edge155::rva::EdgeTabStyleViews_CalculateTargetColors_active_return),
      edge::At(edge155::rva::
                   EdgeTabStyleViews_CalculateTargetColors_inactive_return),
      edge::At(
          edge155::rva::
              EdgeTabStyleViews_PaintInactiveTabBackground_PaintTabBackground_return)};
  for (USHORT i = 0; i < count; i++) {
    for (const void* caller : hoverCallers) {
      if (frames[i] == caller) {
        return true;
      }
    }
  }
  return false;
}
using GetColor_t = bool(__fastcall*)(void*, int, uint32_t*);
inline constinit GetColor_t o_getcolor = nullptr;

inline theme_tone::OptionalColor ThemeColorOf(void* pack, int colorId) {
  theme_tone::OptionalColor color;
  color.isSet = o_getcolor(pack, colorId, &color.color);
  return color;
}

// The skin's colours for the theme in `pack`: its tone, its rail, and the idle
// titles in the theme's tab_background_text (Edge titles every vertical tab in
// tab_text, so a theme cannot dim the idle ones itself).
struct SkinColors {
  theme_tone::Tone themeTone = theme_tone::Tone::kUnknown;
  theme_tone::OptionalColor rail;
  theme_tone::OptionalColor idleLabel;
  theme_tone::OptionalColor activeLabel;
};

inline SkinColors SkinColorsOf(void* pack, bool isFocused) {
  SkinColors colors;
  colors.themeTone =
      theme_tone::ToneOf(ThemeColorOf(pack, edge155::off::THEMECOLOR_toolbar),
                         ThemeColorOf(pack, edge155::off::THEMECOLOR_frame));
  colors.rail = ThemeColorOf(pack, edge155::off::VTABCOLOR_background);
  colors.activeLabel = ThemeColorOf(pack, edge155::off::THEMECOLOR_tab_text);
  // An unfocused window's own idle colour when the theme names one.
  const theme_tone::OptionalColor unfocusedIdleText =
      ThemeColorOf(pack, edge155::off::THEMECOLOR_tab_background_text_inactive);
  const bool isUnfocusedIdleTextUsed = !isFocused && unfocusedIdleText.isSet;
  const theme_tone::OptionalColor idleText =
      isUnfocusedIdleTextUsed
          ? unfocusedIdleText
          : ThemeColorOf(pack, edge155::off::THEMECOLOR_tab_background_text);
  if (colors.rail.isSet) {
    colors.idleLabel =
        cyberpunk_skin::IdleLabelFor(idleText, colors.rail.color);
  }
  return colors;
}

inline bool __fastcall h_getcolor(void* pack, int colorId, uint32_t* out) {
  if (tab_shape::g_isCyberpunkSkinOn) {
    const bool isSelectedTabBackground =
        colorId == edge155::off::VTABCOLOR_selected_background ||
        colorId == edge155::off::VTABCOLOR_selected_background_incognito;
    if (isSelectedTabBackground) {
      const SkinColors colors = SkinColorsOf(pack, focus::IsEdgeForeground());
      skin_tone::g_themeTone = colors.themeTone;
      g_idleLabel = colors.idleLabel;
      // A theme with no rail colour leaves Edge's fill.
      if (!colors.rail.isSet) {
        return o_getcolor(pack, colorId, out);
      }
      // A hover is a wash of the idle titles' colour over the rail, never the
      // accent; any other read is the selected tab, which has no fill.
      const theme_tone::OptionalColor hoverLabel =
          colors.idleLabel.isSet ? colors.idleLabel : colors.activeLabel;
      const bool isHoveredTab = IsReadForHover() && hoverLabel.isSet;
      *out = isHoveredTab ? cyberpunk_skin::HoverFillFor(skin_tone::Current(),
                                                         colors.rail.color,
                                                         hoverLabel.color)
                          : colors.rail.color;
      return true;
    }
  }
  return o_getcolor(pack, colorId, out);
}

// ui::ColorProvider::GetColor: the selected tab's title, close and audio icon in
// the kit's "on" colour. Every vertical tab's title shares one colour id, so
// the title id is inked only while EdgeTab::UpdateForegroundColors
// (h_update_foreground) computes the active tab's colours; that is the pass that
// recolours the tab, so ink and fill switch together. The same pass for an idle
// tab titles it in the skin's idle colour (g_idleLabel). GetColor is
// browser-wide and hot, so the cheap tests go first.
inline constinit void* g_foregroundTab = nullptr;
inline constinit bool g_isForegroundTabActive = false;
inline bool IsActiveTabAlert(int colorId) {
  for (const int alertId : edge155::off::UICOLOR_active_tab_alert) {
    if (colorId == alertId) {
      return true;
    }
  }
  return false;
}
using GetUiColor_t = uint32_t(__fastcall*)(void*, int);
inline constinit GetUiColor_t o_getuicolor = nullptr;
inline uint32_t __fastcall h_getuicolor(void* provider, int colorId) {
  // First, always: a provider generates its colours on its first read, and
  // that is when the mixer reads the theme pack (h_getcolor), so only then do
  // the skin's colours belong to the current theme. Read before it, a fresh
  // theme's tabs kept the previous theme's (or Edge's) titles.
  const uint32_t edgeColor = o_getuicolor(provider, colorId);
  if (colour_dump::g_isTracing) {
    colour_dump::Count(colorId);
  }
  if (!tab_shape::g_isCyberpunkSkinOn) {
    return edgeColor;
  }
  const bool isTabForeground =
      colorId >= edge155::off::UICOLOR_tab_foreground_first &&
      colorId <= edge155::off::UICOLOR_tab_foreground_last;
  const bool isActiveTabTitle = isTabForeground && g_isForegroundTabActive;
  const bool isSelectedTabInk = isActiveTabTitle || IsActiveTabAlert(colorId);
  if (isSelectedTabInk) {
    return cyberpunk_skin::SelectedTabFor(skin_tone::Current()).ink;
  }
  // The browser's own colours, as the kit names them (colour_roles.h): by id
  // lookup, so the common case costs one array read.
  const cyberpunk_palette::Role role = colour_ids::RoleOf(colorId);
  const bool isSkinColour = role != cyberpunk_palette::Role::kNone;
  if (isSkinColour) {
    return cyberpunk_palette::ColourOf(role, skin_tone::Current());
  }
  // Edge's own theme has no pack, so the last pack's idle colour is stale.
  const bool isIdleTabTitle = isTabForeground && g_foregroundTab &&
                              !g_isForegroundTabActive && g_idleLabel.isSet &&
                              !theme::g_isDefaultThemeApplied;
  if (isIdleTabTitle) {
    return g_idleLabel.color;
  }
  return edgeColor;
}

using UpdateForeground_t = void(__fastcall*)(void*);
inline constinit UpdateForeground_t o_update_foreground = nullptr;
inline void __fastcall h_update_foreground(void* tab) {
  void* const outerTab = g_foregroundTab;
  const bool wasOuterActive = g_isForegroundTabActive;
  g_foregroundTab = tab;
  g_isForegroundTabActive = edge::IsTabObjectActive(tab);
  o_update_foreground(tab);
  g_foregroundTab = outerTab;
  g_isForegroundTabActive = wasOuterActive;
}

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
  Install(Feature::kSplit, edge155::rva::chrome_ReloadInternal,
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
  if (availability::IsAvailable(Feature::kWebUiFollowsTheme)) {
    theme_follow::Install();
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
