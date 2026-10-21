// reading_themes.h — the reading mode's page themes. One concern, all of it pure
// (no Edge, no Win32; unit-tested in test/reading_themes_test.cpp): the twelve
// swatches Edge's reading mode offers under Appearance settings > Themes, their
// pref values, their names, and which one a Windows light or dark mode asks for.
//
// The values are Edge's own: the reading_view.theme pref and dom_distiller::Theme
// index learning_tools.dll's theme table ("0":"Sepia" ... "22":"DarkGrey", 23
// entries); the grid shows these twelve of them (docs/re/reading-mode-theme.md).
#pragma once
#include <array>
#include <cstdint>

namespace fixedge::reading_themes {

// Edge's own numbers for each swatch, in the grid's order (row one, then two).
enum class ReadingTheme : int32_t {
  kSepia = 0,
  kLight = 1,
  kGrey = 2,
  kBlue = 5,
  kYellow = 6,
  kGreen = 4,
  kLavender = 18,
  kLightBlue = 17,
  kSkyBlue = 16,
  kLightGreen = 12,
  kBlack = 3,
  kDarkGrey = 22,
};

struct ThemeInfo {
  ReadingTheme theme = ReadingTheme::kSepia;
  // learning_tools' themeName, as Edge's theme table has it.
  const char* name = "";
};

inline constexpr std::array<ThemeInfo, 12> kThemes = {{
    {ReadingTheme::kSepia, "Sepia"},
    {ReadingTheme::kLight, "Light"},
    {ReadingTheme::kGrey, "Grey"},
    {ReadingTheme::kBlue, "Blue"},
    {ReadingTheme::kYellow, "Yellow"},
    {ReadingTheme::kGreen, "Green"},
    {ReadingTheme::kLavender, "Lavender"},
    {ReadingTheme::kLightBlue, "LightBlue"},
    {ReadingTheme::kSkyBlue, "SkyBlue"},
    {ReadingTheme::kLightGreen, "LightGreen"},
    {ReadingTheme::kBlack, "Black"},
    {ReadingTheme::kDarkGrey, "DarkGrey"},
}};

// The theme a number from the wire names, or false when it is not one of the
// twelve (anything else is ignored). Looked up, never cast, so no other number
// ever becomes a ReadingTheme.
[[nodiscard]] constexpr bool ThemeOf(int32_t value,
                                     ReadingTheme* theme) noexcept {
  for (const ThemeInfo& info : kThemes) {
    if (static_cast<int32_t>(info.theme) == value) {
      *theme = info.theme;
      return true;
    }
  }
  return false;
}

[[nodiscard]] constexpr bool IsOffered(int32_t value) noexcept {
  ReadingTheme ignored = ReadingTheme::kSepia;
  return ThemeOf(value, &ignored);
}

// The theme a Windows mode asks for, or false when the choice for it is not one
// of the twelve.
[[nodiscard]] constexpr bool ThemeForMode(bool isDark, int32_t lightChoice,
                                          int32_t darkChoice,
                                          ReadingTheme* theme) noexcept {
  return ThemeOf(isDark ? darkChoice : lightChoice, theme);
}

}  // namespace fixedge::reading_themes
