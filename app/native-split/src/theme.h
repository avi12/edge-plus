// theme.h — the enhanced-theming subsystem: use one theme when Windows is in
// light mode and another when it is in dark mode, and re-apply the moment
// Windows flips, with no browser restart. One concern: picking and applying an
// already installed theme. The panel installs each theme (through Edge's own
// external-extension loader) and records which to use for light and which for
// dark; this module reads that choice and switches between the installed themes.
//
// The OS light/dark signal is the browser window's WM_SETTINGCHANGE (control.h
// forwards it here); the mode itself is read straight from the OS, so no Edge
// theme-observer ABI is touched.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstring>
#include "edge.h"
#include "log.h"

namespace fixedge::theme {

// An extension id (32 lowercase letters) or empty for "use the default theme".
// Sized for the id plus room to spare; never the home of a magic length.
struct ThemeId {
  char value[64] = {};
  size_t length = 0;

  bool IsEmpty() const {
    return length == 0;
  }
};

// The themes assigned to each OS mode, and whether the feature is switched on.
inline ThemeId g_lightTheme;
inline ThemeId g_darkTheme;
inline bool g_isActive = false;

// Where the panel hands this subsystem the two ids until the settings.json
// reader lands. Set once at Edge launch, which is enough to prove the live
// switch (the switch fires on every OS flip, not on a config change).
inline constexpr wchar_t kLightThemeEnv[] = L"FIXEDGE_THEME_LIGHT";
inline constexpr wchar_t kDarkThemeEnv[] = L"FIXEDGE_THEME_DARK";

// Windows is in dark mode when this value is 0 (apps use the light theme when
// it is 1 or absent).
inline bool IsOsDarkMode() {
  DWORD usesLightTheme = 1;
  DWORD size = sizeof(usesLightTheme);
  RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &usesLightTheme, &size);
  return usesLightTheme == 0;
}

inline void ReadIdFromEnv(const wchar_t* name, ThemeId* out) {
  wchar_t wide[64] = {};
  const DWORD wideLength =
      GetEnvironmentVariableW(name, wide, static_cast<DWORD>(std::size(wide)));
  const bool isTooLongOrUnset =
      wideLength == 0 || wideLength >= std::size(wide);
  if (isTooLongOrUnset) {
    *out = {};
    return;
  }
  const int length = WideCharToMultiByte(
      CP_UTF8, 0, wide, static_cast<int>(wideLength), out->value,
      static_cast<int>(std::size(out->value)), nullptr, nullptr);
  out->length = length > 0 ? static_cast<size_t>(length) : 0;
}

inline void LoadConfig() {
  ReadIdFromEnv(kLightThemeEnv, &g_lightTheme);
  ReadIdFromEnv(kDarkThemeEnv, &g_darkTheme);
}

// Apply the theme assigned to the given mode to the window's browser, live.
inline void Apply(void* hwnd, bool isDark) {
  void* tsm = edge::TsmForWindow(hwnd);
  void* themeService = tsm ? edge::ThemeServiceForTabStrip(tsm) : nullptr;
  if (!themeService) {
    Log("[theme] no theme service for hwnd=%p", hwnd);
    return;
  }
  const ThemeId& wanted = isDark ? g_darkTheme : g_lightTheme;
  if (wanted.IsEmpty()) {
    edge::ThemeUseDefault(themeService);
    Log("[theme] %s -> default theme", isDark ? "dark" : "light");
    return;
  }
  edge::ThemeRevertToExtension(themeService, wanted.value, wanted.length);
  Log("[theme] %s -> %.*s", isDark ? "dark" : "light",
      static_cast<int>(wanted.length), wanted.value);
}

// Apply whichever theme matches the current OS mode (a no-op when off).
inline void ApplyForCurrentOsMode(void* hwnd) {
  if (g_isActive) {
    Apply(hwnd, IsOsDarkMode());
  }
}

// Turn the subsystem on: load the assignment and apply it once for the mode
// Windows is in right now. Active only once at least one mode has a theme.
inline void Activate(void* hwnd) {
  LoadConfig();
  g_isActive = !g_lightTheme.IsEmpty() || !g_darkTheme.IsEmpty();
  if (!g_isActive) {
    Log("[theme] no themes assigned; idle");
    return;
  }
  Log("[theme] active (light=%s dark=%s)",
      g_lightTheme.IsEmpty() ? "default" : "set",
      g_darkTheme.IsEmpty() ? "default" : "set");
  ApplyForCurrentOsMode(hwnd);
}

}  // namespace fixedge::theme
