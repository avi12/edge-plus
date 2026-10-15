// theme.h — the enhanced-theming subsystem: use one theme when Windows is in
// light mode and another when it is in dark, and re-apply the moment Windows
// flips, with no browser restart. One concern: picking and applying an already
// installed theme. The panel installs each theme (through Edge's own
// external-extension loader) and pushes which to use for light and which for
// dark (config.h, over WM_COPYDATA); this module switches between them.
//
// The OS light/dark signal is the browser window's WM_SETTINGCHANGE (control.h
// forwards it here); the mode itself is read straight from the OS, so no Edge
// theme-observer ABI is touched.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstring>
#include "config.h"
#include "edge.h"
#include "log.h"

namespace fixedge::theme {

// An extension id (32 lowercase letters) or empty for "use the default theme".
struct ThemeId {
  char value[config::kIdCapacity] = {};
  size_t length = 0;

  bool IsEmpty() const {
    return length == 0;
  }

  void Set(const char* bytes, size_t count) {
    length = count < sizeof(value) ? count : 0;
    if (length > 0) {
      std::memcpy(value, bytes, length);
    }
  }
};

// The themes assigned to each OS mode, and whether the feature is switched on.
inline ThemeId g_lightTheme;
inline ThemeId g_darkTheme;
inline bool g_isActive = false;

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

// Take the panel's latest theming choice and apply it right away. Turning the
// feature off returns both modes to the default theme so nothing we set is left
// behind.
inline void ApplyConfig(void* hwnd, const config::Wire& wire) {
  g_lightTheme.Set(wire.lightId, wire.lightLength);
  g_darkTheme.Set(wire.darkId, wire.darkLength);
  const bool wasActive = g_isActive;
  g_isActive = wire.themingActive != 0;
  if (g_isActive) {
    Log("[theme] active (light=%s dark=%s)",
        g_lightTheme.IsEmpty() ? "default" : "set",
        g_darkTheme.IsEmpty() ? "default" : "set");
    ApplyForCurrentOsMode(hwnd);
    return;
  }
  // Just switched off: drop back to the default theme once.
  if (wasActive) {
    Log("[theme] disabled; reverting to default theme");
    void* tsm = edge::TsmForWindow(hwnd);
    void* themeService = tsm ? edge::ThemeServiceForTabStrip(tsm) : nullptr;
    if (themeService) {
      edge::ThemeUseDefault(themeService);
    }
  }
}

}  // namespace fixedge::theme
