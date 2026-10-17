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

// The new theme lands in the model, but Edge's frame and toolbar only repaint on
// a frame change; a no-move, no-size SWP_FRAMECHANGED makes them redraw at once.
// (When the OS light/dark flip is the trigger Edge repaints anyway; a config
// change pushed from the panel would otherwise lag until the next interaction.)
// The apply settles asynchronously, so we redraw on a one-shot timer that fires
// after it has settled (the timer id is handled in control.h). The frame redraws
// on a frame change; WM_THEMECHANGED is the signal Edge rebuilds its colors from,
// the same one the OS light/dark flip sends — so a switch driven by that flip
// refreshes fully, and a switch pushed from the panel refreshes the frame at
// once and the rest on the next interaction.
inline constexpr UINT kRepaintTimerId = 0xF1EE;
inline constexpr UINT kRepaintDelayMs = 180;

inline void ScheduleRepaint(void* hwnd) {
  SetTimer(static_cast<HWND>(hwnd), kRepaintTimerId, kRepaintDelayMs, nullptr);
}

inline void Repaint(void* hwnd) {
  HWND window = static_cast<HWND>(hwnd);
  KillTimer(window, kRepaintTimerId);
  SetWindowPos(window, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);
  PostMessageW(window, WM_THEMECHANGED, 0, 0);
}

// Apply the theme assigned to the given mode to the window's browser, live.
inline void Apply(void* hwnd, bool isDark) {
  void* tsm = edge::TsmForWindow(hwnd);
  void* profile = tsm ? edge::ProfileForTabStrip(tsm) : nullptr;
  void* themeService =
      profile ? edge::ThemeServiceForProfile(profile) : nullptr;
  if (!themeService) {
    Log("[theme] no theme service for hwnd=%p", hwnd);
    return;
  }
  const ThemeId& wanted = isDark ? g_darkTheme : g_lightTheme;
  if (wanted.IsEmpty()) {
    edge::ThemeUseDefault(themeService);
    ScheduleRepaint(hwnd);
    Log("[theme] %s -> default theme", isDark ? "dark" : "light");
    return;
  }
  edge::ExtensionState state = edge::ExtensionState::kAbsent;
  void* extension = edge::ExtensionById(profile, wanted.value, &state);
  if (!extension) {
    Log("[theme] %s theme %.*s not installed yet", isDark ? "dark" : "light",
        static_cast<int>(wanted.length), wanted.value);
    return;
  }
  // A disabled theme never paints: Edge leaves a theme added from outside off
  // until the user accepts it, and switches the previous one off on each change.
  const bool isDisabled = state == edge::ExtensionState::kDisabled;
  if (isDisabled) {
    edge::EnableDisabledExtension(profile, wanted.value);
  }
  edge::ThemeSetFromExtension(themeService, extension);
  ScheduleRepaint(hwnd);
  Log("[theme] %s -> %.*s applied (was %s)", isDark ? "dark" : "light",
      static_cast<int>(wanted.length), wanted.value,
      isDisabled ? "disabled" : "enabled");
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
    void* profile = tsm ? edge::ProfileForTabStrip(tsm) : nullptr;
    void* themeService =
        profile ? edge::ThemeServiceForProfile(profile) : nullptr;
    if (themeService) {
      edge::ThemeUseDefault(themeService);
    }
  }
}

}  // namespace fixedge::theme
