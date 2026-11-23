// appearance.h — the look of every browser window: the Cyberpunk skin on or
// off, the colours read again after a theme change or a forced tone, the
// reading view's theme for the OS mode, and the followed pages redrawn.
#pragma once
#include <windows.h>
#include "chrome/colours/colour_ids.h"
#include "chrome/tabs/tab_shape.h"
#include "chrome/tabs/tab_views.h"
#include "core/channel.h"
#include "core/log.h"
#include "edge/api/browser.h"
#include "edge/api/theming.h"
#include "edge/api/views.h"
#include "split/control/browser_windows.h"
#include "theme/follow/theme_follow.h"
#include "theme/reading/reading_mode.h"
#include "theme/skin/skin_tone.h"
#include "theme/theme.h"
#include "theme/theme_tone.h"

namespace edgeplus::control {

// `lparam` points at the int counting the pages refreshed so far.
inline BOOL CALLBACK RefreshWindowFollowedPages(HWND hwnd, LPARAM lparam) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* tsm = edge::TsmForWindow(hwnd);
  if (tsm) {
    *reinterpret_cast<int*>(lparam) += theme_follow::RefreshPages(tsm);
  }
  return TRUE;
}

inline BOOL CALLBACK RepaintWindowTabs(HWND hwnd, LPARAM /*unused*/) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* anyView = edge::McvForWindow(hwnd);
  if (anyView) {
    tab_views::RepaintTabs(edge::ViewRoot(anyView));
  }
  return TRUE;
}

// Off is Edge's own look: its tab outline, and its selected tab's colours
// (colour_detours.h h_getcolor), which the theme's re-apply rebuilds.
inline void ApplyCyberpunkSkin(bool isOn) {
  if (tab_shape::g_isCyberpunkSkinOn == isOn) {
    return;
  }
  tab_shape::g_isCyberpunkSkinOn = isOn;
  if (isOn) {
    colour_ids::Resolve();
  }
  EnumWindows(RepaintWindowTabs, 0);
  // Every view reads its colours again, as after a theme change.
  const bool hasWindow = channel::g_window != nullptr;
  if (hasWindow) {
    theme::ScheduleRepaint(channel::g_window);
  }
}

// What a theme change does for every open view: each reads its colours again.
// A script's switches have no theme apply behind them (the panel's do), so they
// ask the default theme again, which notifies the same way.
inline void NotifyColoursChanged(HWND hwnd) {
  void* themeService = edge::ThemeServiceForWindow(hwnd);
  if (themeService) {
    theme::UseDefaultOurselves(themeService);
  }
}

// A script's stand-in for the theme's tone: lParam 1 light, 2 dark, else none.
inline void ForceSkinTone(HWND hwnd, LPARAM lparam) {
  using theme_tone::Tone;
  skin_tone::g_forcedTone = Tone::kUnknown;
  switch (static_cast<skin_tone::ForcedTone>(lparam)) {
    case skin_tone::ForcedTone::kLight:
      skin_tone::g_forcedTone = Tone::kLight;
      break;
    case skin_tone::ForcedTone::kDark:
      skin_tone::g_forcedTone = Tone::kDark;
      break;
    case skin_tone::ForcedTone::kNone:
      break;
  }
  NotifyColoursChanged(hwnd);
}

inline BOOL CALLBACK ApplyWindowReadingTheme(HWND hwnd, LPARAM /*unused*/) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* tsm = edge::TsmForWindow(hwnd);
  if (tsm) {
    reading_mode::ApplyTo(edge::ProfileForTabStrip(tsm));
  }
  return TRUE;
}

inline void ApplyForCurrentOsMode(HWND hwnd) {
  theme::ApplyForCurrentOsMode(hwnd);
  if (reading_mode::g_isActive) {
    EnumWindows(ApplyWindowReadingTheme, 0);
  }
}

inline void RefreshThemeColors() {
  int refreshed = 0;
  EnumWindows(RefreshWindowFollowedPages, reinterpret_cast<LPARAM>(&refreshed));
  Log("[theme-follow] %d open page(s) recomputed their colors", refreshed);
}

}  // namespace edgeplus::control
