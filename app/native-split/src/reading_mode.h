// reading_mode.h — reading mode's page theme follows Windows light and dark. One
// concern: with the panel's switch on, set the reading mode theme the panel
// picked for the current Windows mode, through dom_distiller's own setter
// (edge::SetReadingTheme), which persists it for every later reading view and
// switches the open ones live (docs/re/reading-mode-theme.md). It runs when the
// config arrives and on each Windows light/dark flip, on the same signal theming
// uses (control.h's kApplyTheme); which browser windows (profiles) to set is
// control.h's walk. UI thread only.
#pragma once
#include <cstdint>
#include "config.h"
#include "edge.h"
#include "log.h"
#include "reading_themes.h"
#include "theme.h"

namespace fixedge::reading_mode {

inline bool g_isActive = false;
inline int32_t g_lightTheme = static_cast<int32_t>(
    reading_themes::ReadingTheme::kSepia);
inline int32_t g_darkTheme = static_cast<int32_t>(
    reading_themes::ReadingTheme::kBlack);

// Take the panel's choice; true when the theme should be applied now (switched
// on, or a choice changed while on).
inline bool ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  const int32_t oldLight = g_lightTheme;
  const int32_t oldDark = g_darkTheme;
  g_isActive = wire.readingThemeActive != 0;
  g_lightTheme = static_cast<int32_t>(wire.readingThemeLight);
  g_darkTheme = static_cast<int32_t>(wire.readingThemeDark);
  if (wasActive != g_isActive) {
    Log("[reading] follows Windows %s (light %d, dark %d)",
        g_isActive ? "on" : "off", g_lightTheme, g_darkTheme);
  }
  const bool isChoiceChanged =
      oldLight != g_lightTheme || oldDark != g_darkTheme;
  return g_isActive && (!wasActive || isChoiceChanged);
}

// Set the theme for the current Windows mode in the profile behind
// `browserContext` (a no-op while off or for a choice that is not offered).
inline void ApplyTo(void* browserContext) {
  reading_themes::ReadingTheme theme = reading_themes::ReadingTheme::kSepia;
  const bool isDark = theme::IsOsDarkMode();
  const bool hasTheme =
      g_isActive && browserContext &&
      reading_themes::ThemeForMode(isDark, g_lightTheme, g_darkTheme, &theme);
  if (!hasTheme) {
    return;
  }
  edge::SetReadingTheme(browserContext, static_cast<int32_t>(theme));
  Log("[reading] %s -> theme %d", isDark ? "dark" : "light",
      static_cast<int32_t>(theme));
}

}  // namespace fixedge::reading_mode
