// theme_notice.h — when Edge+ keeps Edge's "Installed theme ... Undo" bubble
// from showing. Pure decision, no Edge and no Win32 (docs/re/theming.md).
//
// Edge shows the bubble from ThemeService::OnThemeBuiltFromExtension whenever a
// theme extension becomes the active one, for every route alike: the panel's
// install through the external-extension loader, Edge+'s own DoSetTheme and the
// light/dark flip all end there, and so does a theme the user installs from a
// store. The theme's extension id is what tells them apart: Edge+ is responsible
// for the two themes it was assigned, and for no other.
#pragma once
#include <string_view>

namespace edgeplus::theme_notice {

// The two themes Edge+ was assigned (config.h), and whether theming is on.
struct Assigned {
  std::string_view lightId;
  std::string_view darkId;
  bool isThemingActive = false;
};

[[nodiscard]] constexpr bool IsAssigned(const Assigned& assigned,
                                        std::string_view themeId) noexcept {
  const bool isNamed = !themeId.empty();
  return isNamed && (themeId == assigned.lightId || themeId == assigned.darkId);
}

// True when the bubble for `themeId` must not show: Edge+ applies that theme
// itself. Edge already asked for no bubble (`isAlreadySuppressed`) is kept.
[[nodiscard]] constexpr bool ShouldSuppress(const Assigned& assigned,
                                            std::string_view themeId,
                                            bool isAlreadySuppressed) noexcept {
  if (isAlreadySuppressed) {
    return true;
  }
  return assigned.isThemingActive && IsAssigned(assigned, themeId);
}

}  // namespace edgeplus::theme_notice
