// theme_tone.h — is an Edge theme dark or light? A theme extension says nothing
// about it, so the tone is read off its colors: the toolbar first (the largest
// surface a theme always colors), else the frame. Pure.
#pragma once
#include <cstdint>

namespace fixedge::theme_tone {

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

}  // namespace fixedge::theme_tone
