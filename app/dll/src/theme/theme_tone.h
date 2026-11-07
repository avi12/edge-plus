// theme_tone.h — is an Edge theme dark or light? A theme extension says nothing
// about it, so the tone is read off its colors: the toolbar first (the largest
// surface a theme always colors), else the frame. Also how two of its colours
// blend and whether text in one reads on the other. Pure.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace edgeplus::theme_tone {

// 0xAARRGGBB as Skia stores it (also a Windows BGRA pixel).
using Argb = uint32_t;

inline constexpr int kAlphaShift = 24;
inline constexpr int kRedShift = 16;
inline constexpr int kGreenShift = 8;
inline constexpr int kBlueShift = 0;
inline constexpr uint32_t kChannelMask = 0xff;
inline constexpr uint32_t kChannelMax = 0xff;

// Chromium's color_utils::IsDark, with Rec. 601 luma weights.
inline constexpr int kRedWeight = 299;
inline constexpr int kGreenWeight = 587;
inline constexpr int kBlueWeight = 114;
inline constexpr int kWeightScale = 1000;
inline constexpr int kDarkLumaBelow = 128;

[[nodiscard]] constexpr int Luma(Argb color) noexcept {
  const int red = static_cast<int>((color >> kRedShift) & kChannelMask);
  const int green = static_cast<int>((color >> kGreenShift) & kChannelMask);
  const int blue = static_cast<int>(color & kChannelMask);
  return (red * kRedWeight + green * kGreenWeight + blue * kBlueWeight) /
         kWeightScale;
}

[[nodiscard]] constexpr bool IsDark(Argb color) noexcept {
  return Luma(color) < kDarkLumaBelow;
}

// Opaque; how a window that lost focus dims a colour into its surroundings.
inline constexpr uint32_t kPercentScale = 100;
[[nodiscard]] constexpr Argb Blend(Argb from, Argb to,
                                   uint32_t toPercent) noexcept {
  const auto mix = [&](int shift) {
    const uint32_t fromChannel = (from >> shift) & kChannelMask;
    const uint32_t toChannel = (to >> shift) & kChannelMask;
    return ((fromChannel * (kPercentScale - toPercent) +
             toChannel * toPercent) /
            kPercentScale)
           << shift;
  };
  return 0xff000000u | mix(kRedShift) | mix(kGreenShift) | mix(kBlueShift);
}

// WCAG 2's contrast ratio, 1 (none) to 21 (black on white); text below
// kReadableContrast fails its AA minimum.
inline constexpr double kReadableContrast = 4.5;
inline constexpr double kChannelUnit = 255.0;
inline constexpr double kLinearBelow = 0.04045;
inline constexpr double kLinearSlope = 12.92;
inline constexpr double kGammaOffset = 0.055;
inline constexpr double kGammaScale = 1.055;
inline constexpr double kGamma = 2.4;
inline constexpr double kRedLuminance = 0.2126;
inline constexpr double kGreenLuminance = 0.7152;
inline constexpr double kBlueLuminance = 0.0722;
inline constexpr double kFlare = 0.05;

[[nodiscard]] inline double RelativeLuminance(Argb color) noexcept {
  const auto linear = [&](int shift) {
    const double channel =
        static_cast<double>((color >> shift) & kChannelMask) / kChannelUnit;
    return channel <= kLinearBelow
               ? channel / kLinearSlope
               : std::pow((channel + kGammaOffset) / kGammaScale, kGamma);
  };
  return kRedLuminance * linear(kRedShift) +
         kGreenLuminance * linear(kGreenShift) +
         kBlueLuminance * linear(kBlueShift);
}

[[nodiscard]] inline double ContrastRatio(Argb first, Argb second) noexcept {
  const double firstLuminance = RelativeLuminance(first);
  const double secondLuminance = RelativeLuminance(second);
  const double lighter = std::max(firstLuminance, secondLuminance);
  const double darker = std::min(firstLuminance, secondLuminance);
  return (lighter + kFlare) / (darker + kFlare);
}

enum class Tone {
  kUnknown,  // the theme colored neither the toolbar nor the frame
  kLight,
  kDark,
};

struct OptionalColor {
  bool isSet = false;
  Argb color = 0;
};

[[nodiscard]] constexpr Tone ToneOf(OptionalColor toolbar,
                                    OptionalColor frame) noexcept {
  if (toolbar.isSet) {
    return IsDark(toolbar.color) ? Tone::kDark : Tone::kLight;
  }
  if (frame.isSet) {
    return IsDark(frame.color) ? Tone::kDark : Tone::kLight;
  }
  return Tone::kUnknown;
}

}  // namespace edgeplus::theme_tone
