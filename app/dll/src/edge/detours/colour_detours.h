// colour_detours.h — the detours on the colours Edge paints the vertical tab
// list with, for the Cyberpunk skin: a theme pack's colours (the rail, the
// idle titles, the hover) and the selected tab's ink.
#pragma once
#include <windows.h>
#include <cstdint>
#include "chrome/colours/colour_dump.h"
#include "chrome/colours/colour_ids.h"
#include "chrome/tabs/tab_shape.h"
#include "core/focus.h"
#include "edge/api/binding.h"
#include "edge/api/tab_objects.h"
#include "edge/offsets.h"
#include "theme/skin/cyberpunk_palette.h"
#include "theme/skin/cyberpunk_skin.h"
#include "theme/skin/skin_tone.h"
#include "theme/theme.h"
#include "theme/theme_tone.h"

namespace edgeplus::hooks {

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
  const bool isIdleTabTitle = isTabForeground && (g_foregroundTab != nullptr) &&
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

}  // namespace edgeplus::hooks
