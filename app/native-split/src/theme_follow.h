// theme_follow.h — make edge://settings and edge://extensions show the same
// light or dark as the browser frame. One concern: those two pages' light/dark,
// and nothing else's. See docs/re/webui-color-scheme.md.
//
// Edge draws a page light or dark from two things, neither of which reads the
// theme: its prefers-color-scheme (computed per WebContents) and, for a WebUI
// page, the colors of its color provider, whose mode is Windows' or the
// Appearance setting's. A dark theme on a light Windows therefore leaves the
// frame dark and the settings page white. While the feature is on, a profile
// painted by a theme extension gives those two pages the tone of the theme's own
// colors (theme_tone.h) in both. Every other page, ordinary or WebUI, and a
// profile on the default theme, keep what Edge decides.
#pragma once
#include "MinHook.h"
#include "config.h"
#include "edge.h"
#include "channel.h"
#include "followed_pages.h"
#include "log.h"
#include "theme_tone.h"

namespace fixedge::theme_follow {

inline constinit bool g_isActive = false;

// The tone a followed page was last computed in, and when a redraw was last
// asked for: a theme swap changes the tone a page should have, but the page
// keeps the colors it loaded with, so the first computation that sees a new tone
// asks for a redraw (control.h), at most once in kRedrawCooldownMs so a page
// that cannot take the tone is not reloaded forever.
inline constexpr ULONGLONG kRedrawCooldownMs = 3000;
inline constexpr int32_t kNoModeYet = -1;
inline constinit int32_t g_lastPreferredMode = kNoModeYet;
inline constinit ULONGLONG g_lastRedrawAskedAt = 0;

// The color mode the provider key must carry for the one synchronous
// GetColorProvider call that is building a followed page's colors, or
// kNoForcedMode.
inline constexpr int32_t kNoForcedMode = -1;
inline constinit int32_t g_forcedColorMode = kNoForcedMode;

// The tone the active theme gives the page in `webContents`, when the feature is
// on, the page is one of the two, and the theme (an extension or colored theme)
// set the colors that say so.
inline theme_tone::Tone ToneForPage(void* webContents) {
  if (!g_isActive || !webContents) {
    return theme_tone::Tone::kUnknown;
  }
  const bool isFollowedPage =
      followed_pages::Follows(edge::VisibleUrl(webContents));
  if (!isFollowedPage) {
    return theme_tone::Tone::kUnknown;
  }
  void* profile = edge::BrowserContextOf(webContents);
  void* themeService =
      profile ? edge::ThemeServiceForProfile(profile) : nullptr;
  if (!themeService) {
    return theme_tone::Tone::kUnknown;
  }
  return edge::ThemeTone(themeService);
}

// EdgeComputePreferredColorScheme: the page's prefers-color-scheme (and root
// scrollbar scheme), as Edge computed them, unless the page follows the theme.
using Preferred_t = void*(__fastcall*)(void*, const void*, void*);
inline constinit Preferred_t o_preferred = nullptr;

inline void* __fastcall h_preferred(void* out, const void* siteInfo,
                                    void* webContents) {
  void* result = o_preferred(out, siteInfo, webContents);
  const theme_tone::Tone tone = ToneForPage(webContents);
  if (tone == theme_tone::Tone::kUnknown) {
    return result;
  }
  const bool isDark = tone == theme_tone::Tone::kDark;
  const int32_t mode =
      isDark ? edge155::off::COLORMODE_dark : edge155::off::COLORMODE_light;
  const bool isToneChanged =
      g_lastPreferredMode != kNoModeYet && g_lastPreferredMode != mode;
  g_lastPreferredMode = mode;
  const bool isCooledDown =
      GetTickCount64() - g_lastRedrawAskedAt > kRedrawCooldownMs;
  if (isToneChanged && isCooledDown) {
    g_lastRedrawAskedAt = GetTickCount64();
    channel::Post(channel::CodeOf(channel::Command::kRedrawFollowedPages));
  }
  // Blink's PreferredColorScheme is 0 for dark and 1 for light; the scrollbar
  // scheme is the other way round.
  edge::Field<int32_t>(out, edge155::off::PREFERREDSCHEME_color_scheme) =
      isDark ? 0 : 1;
  edge::Field<int32_t>(out, edge155::off::PREFERREDSCHEME_scrollbar_scheme) =
      isDark ? 1 : 0;
  return result;
}

// ThemeService::GetColorProviderKey: Edge's key, with the mode replaced while
// a followed page's provider is being asked for.
using ProviderKey_t = void*(__fastcall*)(void*, void*, const void*, void*);
inline constinit ProviderKey_t o_provider_key = nullptr;

inline void* __fastcall h_provider_key(void* themeService, void* key,
                                       const void* nativeKey, void* profile) {
  void* result = o_provider_key(themeService, key, nativeKey, profile);
  const bool isForced = g_forcedColorMode != kNoForcedMode;
  if (isForced) {
    edge::Field<int32_t>(result, edge155::off::COLORPROVIDERKEY_color_mode) =
        g_forcedColorMode;
  }
  return result;
}

// WebContentsImpl::GetColorProvider: the provider the page's colors.css is
// built from. A followed page gets the one for the theme's tone, by asking for
// it with the key's mode forced for the duration of the call.
using WebContentsProvider_t = const void*(__fastcall*)(void*);
inline constinit WebContentsProvider_t o_contents_provider = nullptr;

inline const void* __fastcall h_contents_provider(void* webContents) {
  const theme_tone::Tone tone = ToneForPage(webContents);
  if (tone == theme_tone::Tone::kUnknown) {
    return o_contents_provider(webContents);
  }
  const bool isDark = tone == theme_tone::Tone::kDark;
  g_forcedColorMode =
      isDark ? edge155::off::COLORMODE_dark : edge155::off::COLORMODE_light;
  const void* provider = o_contents_provider(webContents);
  g_forcedColorMode = kNoForcedMode;
  return provider;
}

// Make every open followed page in the window whose tab strip is `tsm` take up a
// change of the switch, at once. Its prefers-color-scheme follows a notice that
// its color provider changed, but a WebUI page draws its colors from a style
// sheet it was loaded with (Edge's own Appearance setting leaves an open page's
// colors as they were, too), so it is reloaded to be drawn in the new colors.
// Returns how many pages that was.
inline int RefreshPages(void* tsm) {
  int refreshed = 0;
  const int tabCount = edge::TabCount(tsm);
  for (int i = 0; i < tabCount; i++) {
    void* webContents = edge::TabWebContentsAt(tsm, i);
    const bool isFollowedPage =
        webContents && followed_pages::Follows(edge::VisibleUrl(webContents));
    if (isFollowedPage) {
      edge::NotifyColorProviderChanged(webContents);
      edge::ReloadPage(webContents);
      refreshed++;
    }
  }
  return refreshed;
}

// --- install / apply -----------------------------------------------------

inline bool EnableSite(uintptr_t site) {
  const MH_STATUS status = MH_EnableHook(edge::At(site));
  return status == MH_OK || status == MH_ERROR_ENABLED;
}

inline bool EnableAll() {
  return EnableSite(edge155::rva::EdgeComputePreferredColorScheme) &&
         EnableSite(edge155::rva::WebContentsImpl_GetColorProvider) &&
         EnableSite(edge155::rva::ThemeService_GetColorProviderKey);
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(edge::At(edge155::rva::EdgeComputePreferredColorScheme),
                    reinterpret_cast<void*>(h_preferred),
                    reinterpret_cast<void**>(&o_preferred)) == MH_OK &&
      MH_CreateHook(edge::At(edge155::rva::WebContentsImpl_GetColorProvider),
                    reinterpret_cast<void*>(h_contents_provider),
                    reinterpret_cast<void**>(&o_contents_provider)) == MH_OK &&
      MH_CreateHook(edge::At(edge155::rva::ThemeService_GetColorProviderKey),
                    reinterpret_cast<void*>(h_provider_key),
                    reinterpret_cast<void**>(&o_provider_key)) == MH_OK;
  if (!isHooked) {
    Log("[err] theme-follow hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] theme-follow hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

// Take the panel's switch. Returns true when it changed, so the caller asks
// every open profile to compute its pages again.
inline bool ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  g_isActive = wire.webUiFollowsThemeActive != 0;
  const bool isChanged = wasActive != g_isActive;
  if (isChanged) {
    g_lastPreferredMode = kNoModeYet;
    Log("[theme-follow] %s", g_isActive ? "on" : "off");
  }
  return isChanged;
}

}  // namespace fixedge::theme_follow
