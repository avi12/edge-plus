// theme_follow.h — make edge://settings and edge://extensions (Edge's WebUI
// pages) show the same light or dark as the browser frame. One concern: the
// answer to "does the browser use dark colors", which those pages' light/dark
// comes from. See docs/re/webui-color-scheme.md.
//
// Edge answers that question from its Appearance setting, or from Windows when
// the setting is "System default", and never from the theme: a dark theme on a
// light Windows leaves the frame dark and the settings page white. While the
// feature is on, a profile painted by a theme extension answers with the tone of
// the theme's own colors instead (theme_tone.h). A profile on the default theme
// keeps Edge's answer, so Edge's own Appearance setting still rules there.
#pragma once
#include "MinHook.h"
#include "config.h"
#include "edge.h"
#include "log.h"
#include "theme_tone.h"

namespace fixedge::theme_follow {

using namespace fixedge::edge155;

inline bool g_isActive = false;

// The answer most recently given while on, to log each change once (the hook
// runs on every color-provider and page-preference computation).
inline int g_lastLoggedAnswer = -1;

// The tone the active theme's own colors give the browser, when the feature is
// on and the theme (an extension or colored theme) set the colors that say so.
inline theme_tone::Tone ActiveTone(void* themeService) {
  if (!g_isActive) {
    return theme_tone::Tone::kUnknown;
  }
  return theme_tone::ToneOf(
      edge::ThemePaletteColor(themeService, off::THEMECOLOR_toolbar),
      edge::ThemePaletteColor(themeService, off::THEMECOLOR_frame));
}

// ThemeService::GetColorProviderKey: the color provider's mode, from which the
// pages' prefers-color-scheme follows. Edge fills it from the OS or the
// Appearance setting; with the feature on, a theme's tone replaces that.
using ProviderKey_t = void*(__fastcall*)(void*, void*, const void*, void*);
inline ProviderKey_t o_provider_key = nullptr;

inline void* __fastcall h_provider_key(void* themeService, void* key,
                                       const void* nativeKey, void* profile) {
  void* result = o_provider_key(themeService, key, nativeKey, profile);
  const theme_tone::Tone tone = ActiveTone(themeService);
  if (tone != theme_tone::Tone::kUnknown) {
    const bool isThemeDark = tone == theme_tone::Tone::kDark;
    edge::Field<int32_t>(result, off::COLORPROVIDERKEY_color_mode) =
        isThemeDark ? off::COLORMODE_dark : off::COLORMODE_light;
  }
  return result;
}

// ThemeService::BrowserUsesDarkColors: the same question asked directly by
// Edge's own UI (icons, the find bar, the new tab page), answered the same way.
using UsesDark_t = bool(__fastcall*)(void*);
inline UsesDark_t o_uses_dark = nullptr;

inline bool __fastcall h_uses_dark(void* themeService) {
  const bool isEdgeDark = o_uses_dark(themeService);
  const theme_tone::Tone tone = ActiveTone(themeService);
  if (tone == theme_tone::Tone::kUnknown) {
    return isEdgeDark;
  }
  const bool isThemeDark = tone == theme_tone::Tone::kDark;
  const int answer = (isThemeDark ? 1 : 0) | (isEdgeDark ? 2 : 0);
  if (answer != g_lastLoggedAnswer) {
    g_lastLoggedAnswer = answer;
    Log("[theme-follow] theme is %s, Edge said %s -> %s",
        isThemeDark ? "dark" : "light", isEdgeDark ? "dark" : "light",
        isThemeDark ? "dark" : "light");
  }
  return isThemeDark;
}

// --- install / apply -----------------------------------------------------

inline bool IsEnabled(uintptr_t site) {
  const MH_STATUS status = MH_EnableHook(edge::At(site));
  return status == MH_OK || status == MH_ERROR_ENABLED;
}

inline bool EnableAll() {
  return IsEnabled(rva::ThemeService_GetColorProviderKey) &&
         IsEnabled(rva::ThemeService_BrowserUsesDarkColors);
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(edge::At(rva::ThemeService_GetColorProviderKey),
                    reinterpret_cast<void*>(h_provider_key),
                    reinterpret_cast<void**>(&o_provider_key)) == MH_OK &&
      MH_CreateHook(edge::At(rva::ThemeService_BrowserUsesDarkColors),
                    reinterpret_cast<void*>(h_uses_dark),
                    reinterpret_cast<void**>(&o_uses_dark)) == MH_OK;
  if (!isHooked) {
    Log("[err] theme-follow hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] theme-follow hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

// Take the panel's switch. Returns true when it changed, so the caller asks
// every open profile to compute its colors again.
inline bool ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  g_isActive = wire.webUiFollowsThemeActive != 0;
  g_lastLoggedAnswer = -1;
  const bool isChanged = wasActive != g_isActive;
  if (isChanged) {
    Log("[theme-follow] %s", g_isActive ? "on" : "off");
  }
  return isChanged;
}

}  // namespace fixedge::theme_follow
