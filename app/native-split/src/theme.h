// theme.h — one installed theme for Windows' light mode and another for dark,
// re-applied live the moment Windows flips. The panel installs the themes and
// pushes the choice (config.h); control.h forwards WM_SETTINGCHANGE here, and
// the mode is read straight from the OS, so no Edge theme-observer ABI is
// touched.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstring>
#include "channel.h"
#include "config.h"
#include "edge.h"
#include "log.h"

namespace edgeplus::theme {

// An extension id, or empty for the default theme.
struct ThemeId {
  char value[config::kIdCapacity] = {};
  size_t length = 0;

  [[nodiscard]] bool IsEmpty() const {
    return length == 0;
  }

  void Set(const char* bytes, size_t count) {
    length = count < sizeof(value) ? count : 0;
    if (length > 0) {
      std::memcpy(value, bytes, length);
    }
  }
};

inline constinit ThemeId g_lightTheme;
inline constinit ThemeId g_darkTheme;
inline constinit bool g_isActive = false;

// AppsUseLightTheme absent means light.
inline bool IsOsDarkMode() {
  DWORD usesLightTheme = 1;
  DWORD size = sizeof(usesLightTheme);
  RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &usesLightTheme, &size);
  return usesLightTheme == 0;
}

// Edge's frame and toolbar only repaint on a frame change, so a theme pushed
// from the panel would lag until the next interaction: SWP_FRAMECHANGED redraws
// the frame and WM_THEMECHANGED (what the OS flip sends) makes Edge rebuild its
// colors. The apply settles asynchronously, hence a one-shot timer (control.h).
inline constexpr UINT_PTR kRepaintTimerId =
    channel::TimerIdOf(channel::WindowTimer::kThemeRepaint);
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

// A reinstall lands asynchronously, so the apply is retried a few times.
inline constexpr UINT kInstallRetryDelayMs = 1500;
inline constexpr int kMaxInstallRetries = 6;
inline constinit int g_installRetries = 0;

// A thread timer, not a window one: a fresh launch was seen to lose the window
// timer, leaving the dark theme uninstalled until the next config change. The
// callback only posts to the channel, which applies on the UI thread.
inline void CALLBACK InstallRetryFired(HWND, UINT, UINT_PTR timerId, DWORD) {
  KillTimer(nullptr, timerId);
  channel::Post(channel::CodeOf(channel::Command::kApplyTheme));
}

inline void ScheduleInstallRetry() {
  SetTimer(nullptr, 0, kInstallRetryDelayMs, InstallRetryFired);
}

inline void Apply(void* hwnd, bool isDark) {
  void* profile = edge::ProfileForTabStrip(edge::TsmForWindow(hwnd));
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
    // Missing, not merely late: a theme the user once removed stays removed
    // (Edge records it as externally uninstalled), so ask for it back.
    edge::ReinstallExternalExtension(profile, wanted.value);
    const bool canRetry = g_installRetries < kMaxInstallRetries;
    if (canRetry) {
      g_installRetries++;
      ScheduleInstallRetry();
    }
    Log("[theme] %s theme %.*s not installed yet; asked Edge to install it "
        "(try %d of %d)",
        isDark ? "dark" : "light", static_cast<int>(wanted.length),
        wanted.value, g_installRetries, kMaxInstallRetries);
    return;
  }
  g_installRetries = 0;
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

inline void ApplyForCurrentOsMode(void* hwnd) {
  if (g_isActive) {
    Apply(hwnd, IsOsDarkMode());
  }
}

// Turning the feature off returns to the default theme, so nothing we set is
// left behind.
inline void ApplyConfig(void* hwnd, const config::Wire& wire) {
  g_lightTheme.Set(wire.lightId, wire.lightLength);
  g_darkTheme.Set(wire.darkId, wire.darkLength);
  g_installRetries = 0;
  const bool wasActive = g_isActive;
  g_isActive = wire.themingActive != 0;
  if (g_isActive) {
    Log("[theme] active (light=%s dark=%s)",
        g_lightTheme.IsEmpty() ? "default" : "set",
        g_darkTheme.IsEmpty() ? "default" : "set");
    ApplyForCurrentOsMode(hwnd);
    return;
  }
  if (!wasActive) {
    return;
  }
  Log("[theme] disabled; reverting to default theme");
  void* themeService = edge::ThemeServiceForWindow(hwnd);
  if (themeService) {
    edge::ThemeUseDefault(themeService);
  }
}

}  // namespace edgeplus::theme
