// remix_colors.h — the pair of colours an Appearance remix can wear: one for
// Edge's light mode and one for its dark mode, as the Edge+ theme has a light
// theme and a dark theme (docs/re/remix-theme.md). A colour is the editor's own
// two numbers, the wheel's hue and its intensity. The pair is kept in a file of
// the DLL's own (remix_colors_store.h) and travels between the editor page and
// the DLL as text, in the page's address fragment. Pure.
#pragma once
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include "theme/theme_tone.h"

namespace edgeplus::remix_colors {

inline constexpr int kHueDegrees = 360;
inline constexpr int kIntensityMax = 100;

// What the page puts in its address after the page's own, so the DLL can read
// it back without a script answer.
inline constexpr std::string_view kFragmentKey = "#edgeplus-remix=";

// After the pair the open editor names the mode it previews, the mode the wheel
// edits and the page shows: "...;dark=7,8;preview=dark". Empty while the
// Appearance section is not shown.
inline constexpr std::string_view kPreviewKey = ";preview=";
inline constexpr std::string_view kLightName = "light";
inline constexpr std::string_view kDarkName = "dark";

struct Colour {
  int hue = 0;        // degrees, 0..359
  int intensity = 0;  // percent, 0..100
  bool isSet = false;
  bool operator==(const Colour&) const = default;
};

struct Pair {
  Colour light;
  Colour dark;
  bool operator==(const Pair&) const = default;
};

[[nodiscard]] constexpr const Colour& ColourFor(const Pair& pair,
                                                bool isDark) noexcept {
  return isDark ? pair.dark : pair.light;
}

// A new appearance's editor holds no colour until the user picks one; a pair
// with none is never saved, so cancelling it keeps the saved pair.
[[nodiscard]] constexpr bool HasColour(const Pair& pair) noexcept {
  return pair.light.isSet || pair.dark.isSet;
}

namespace detail {

constexpr char kSideSeparator = ';';
constexpr char kNumberSeparator = ',';
constexpr std::string_view kLightKey = "light=";
constexpr std::string_view kDarkKey = "dark=";

[[nodiscard]] inline bool ParseNumber(std::string_view text, int minimum,
                                      int maximum, int* number) {
  int value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  const bool isNumber =
      error == std::errc() && end == text.data() + text.size();
  if (!isNumber || value < minimum || value > maximum) {
    return false;
  }
  *number = value;
  return true;
}

// "" is an unset colour; "210,40" a set one.
[[nodiscard]] inline bool ParseColour(std::string_view text, Colour* colour) {
  if (text.empty()) {
    *colour = Colour{};
    return true;
  }
  const size_t comma = text.find(kNumberSeparator);
  if (comma == std::string_view::npos) {
    return false;
  }
  Colour parsed;
  parsed.isSet = true;
  if (!ParseNumber(text.substr(0, comma), 0, kHueDegrees - 1, &parsed.hue) ||
      !ParseNumber(text.substr(comma + 1), 0, kIntensityMax,
                   &parsed.intensity)) {
    return false;
  }
  *colour = parsed;
  return true;
}

[[nodiscard]] inline std::string FormatColour(const Colour& colour) {
  if (!colour.isSet) {
    return std::string();
  }
  return std::to_string(colour.hue) + kNumberSeparator +
         std::to_string(colour.intensity);
}

}  // namespace detail

// "light=210,40;dark=" — an unset side is empty.
[[nodiscard]] inline std::string FormatPair(const Pair& pair) {
  std::string text(detail::kLightKey);
  text += detail::FormatColour(pair.light);
  text += detail::kSideSeparator;
  text += detail::kDarkKey;
  text += detail::FormatColour(pair.dark);
  return text;
}

[[nodiscard]] inline bool ParsePair(std::string_view text, Pair* pair) {
  const size_t separator = text.find(detail::kSideSeparator);
  const bool isShaped = separator != std::string_view::npos &&
                        text.starts_with(detail::kLightKey);
  if (!isShaped) {
    return false;
  }
  const std::string_view lightText = text.substr(
      detail::kLightKey.size(), separator - detail::kLightKey.size());
  const std::string_view rest = text.substr(separator + 1);
  if (!rest.starts_with(detail::kDarkKey)) {
    return false;
  }
  // The dark side ends at the next field, when the text carries one.
  const std::string_view darkText =
      rest.substr(detail::kDarkKey.size(),
                  rest.find(detail::kSideSeparator) - detail::kDarkKey.size());
  Pair parsed;
  if (!detail::ParseColour(lightText, &parsed.light) ||
      !detail::ParseColour(darkText, &parsed.dark)) {
    return false;
  }
  *pair = parsed;
  return true;
}

// The pair in a page address, or false when it carries none.
[[nodiscard]] inline bool PairFromUrl(std::string_view url, Pair* pair) {
  const size_t fragmentAt = url.find(kFragmentKey);
  if (fragmentAt == std::string_view::npos) {
    return false;
  }
  return ParsePair(url.substr(fragmentAt + kFragmentKey.size()), pair);
}

// The mode the editor in `url` previews: kUnknown when it names none.
[[nodiscard]] inline theme_tone::Tone PreviewFromUrl(std::string_view url) {
  const size_t fragmentAt = url.find(kFragmentKey);
  if (fragmentAt == std::string_view::npos) {
    return theme_tone::Tone::kUnknown;
  }
  const size_t previewAt = url.find(kPreviewKey, fragmentAt);
  if (previewAt == std::string_view::npos) {
    return theme_tone::Tone::kUnknown;
  }
  const std::string_view rest = url.substr(previewAt + kPreviewKey.size());
  const std::string_view name =
      rest.substr(0, rest.find(detail::kSideSeparator));
  if (name == kLightName) {
    return theme_tone::Tone::kLight;
  }
  if (name == kDarkName) {
    return theme_tone::Tone::kDark;
  }
  return theme_tone::Tone::kUnknown;
}

}  // namespace edgeplus::remix_colors
