// reading_themes.h — the twelve page themes Edge's reading mode offers, the one
// picked for each Windows mode, and that choice's file and page-answer forms.
// Pure.
//
// The values are Edge's own: the reading_view.theme pref and dom_distiller::Theme
// index learning_tools.dll's theme table ("0":"Sepia" ... "22":"DarkGrey", 23
// entries); the grid shows these twelve of them (docs/re/reading-mode-theme.md).
#pragma once
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <system_error>

namespace fixedge::reading_themes {

// In the grid's order.
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

// Looked up, never cast, so no other number ever becomes a ReadingTheme.
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

struct Choices {
  ReadingTheme light = ReadingTheme::kSepia;
  ReadingTheme dark = ReadingTheme::kBlack;
  bool operator==(const Choices&) const = default;
};

[[nodiscard]] constexpr ReadingTheme ForMode(const Choices& choices,
                                             bool isDark) noexcept {
  return isDark ? choices.dark : choices.light;
}

// The light theme's number, then the dark one's, each a little-endian int32.
inline constexpr size_t kChoicesFileSize = 8;
inline constexpr int kByteBits = 8;
inline constexpr uint32_t kByteMask = 0xff;

[[nodiscard]] constexpr std::array<uint8_t, kChoicesFileSize> BytesOf(
    const Choices& choices) noexcept {
  std::array<uint8_t, kChoicesFileSize> bytes{};
  const std::array<ReadingTheme, 2> sides = {choices.light, choices.dark};
  for (size_t side = 0; side < sides.size(); side++) {
    const auto value = static_cast<uint32_t>(sides[side]);
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
      bytes[side * sizeof(uint32_t) + i] =
          static_cast<uint8_t>((value >> (i * kByteBits)) & kByteMask);
    }
  }
  return bytes;
}

// A file of another size, or an unknown number, leaves that side at its default.
[[nodiscard]] constexpr Choices ChoicesFromBytes(
    std::span<const uint8_t> bytes) noexcept {
  Choices choices;
  if (bytes.size() != kChoicesFileSize) {
    return choices;
  }
  const auto valueAt = [&](size_t offset) {
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
      value |= static_cast<uint32_t>(bytes[offset + i]) << (i * kByteBits);
    }
    return static_cast<int32_t>(value);
  };
  ReadingTheme theme = ReadingTheme::kSepia;
  if (ThemeOf(valueAt(0), &theme)) {
    choices.light = theme;
  }
  if (ThemeOf(valueAt(sizeof(uint32_t)), &theme)) {
    choices.dark = theme;
  }
  return choices;
}

[[nodiscard]] inline bool ChoicesFromFields(std::string_view light,
                                            std::string_view dark,
                                            Choices* choices) noexcept {
  const auto themeIn = [](std::string_view text, ReadingTheme* theme) {
    int32_t value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    const bool isNumber =
        error == std::errc() && end == text.data() + text.size();
    return isNumber && ThemeOf(value, theme);
  };
  Choices parsed;
  if (!themeIn(light, &parsed.light) || !themeIn(dark, &parsed.dark)) {
    return false;
  }
  *choices = parsed;
  return true;
}

}  // namespace fixedge::reading_themes
