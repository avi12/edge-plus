// theme_tone.h — is an Edge theme dark or light? One concern: the pure decision
// from a theme's own colors, so the WebUI pages (settings, extensions) can be
// told the same light/dark the browser frame shows. No Edge, no Win32.
//
// A theme extension paints the frame and toolbar with its own colors and says
// nothing about light or dark, so the tone is read off those colors: the toolbar
// first (the largest surface a theme always colors), the frame when it left the
// toolbar alone, and no verdict when it set neither.
#pragma once
#include <cstdint>

namespace fixedge::theme_tone {

// A theme's color as Skia stores it: 0xAARRGGBB (also a Windows BGRA pixel).
using Argb = uint32_t;

inline constexpr int kAlphaShift = 24;
inline constexpr int kRedShift = 16;
inline constexpr int kGreenShift = 8;
inline constexpr int kBlueShift = 0;
inline constexpr uint32_t kChannelMask = 0xff;
// A channel at full strength (an opaque alpha).
inline constexpr uint32_t kChannelMax = 0xff;

// Chromium's color_utils::IsDark: a color is dark when its luma (the weighted
// sum of its channels, out of 255) is below the middle. The weights are the
// Rec. 601 ones, in thousandths.
inline constexpr int kRedWeight = 299;
inline constexpr int kGreenWeight = 587;
inline constexpr int kBlueWeight = 114;
inline constexpr int kWeightScale = 1000;
inline constexpr int kDarkLumaBelow = 128;

constexpr int Luma(Argb color) {
  const int red = static_cast<int>((color >> kRedShift) & kChannelMask);
  const int green = static_cast<int>((color >> kGreenShift) & kChannelMask);
  const int blue = static_cast<int>(color & kChannelMask);
  return (red * kRedWeight + green * kGreenWeight + blue * kBlueWeight) /
         kWeightScale;
}

constexpr bool IsDark(Argb color) {
  return Luma(color) < kDarkLumaBelow;
}

// `from` moved `toPercent` percent of the way toward `to`, opaque: how a window
// that lost focus dims a colour into its surroundings.
inline constexpr uint32_t kPercentScale = 100;
constexpr Argb Blend(Argb from, Argb to, uint32_t toPercent) {
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

// What a theme's palette says about its own tone.
enum class Tone {
  kUnknown,  // the theme colored neither the toolbar nor the frame
  kLight,
  kDark,
};

// A color a theme may or may not have set.
struct OptionalColor {
  bool isSet = false;
  Argb color = 0;
};

constexpr Tone ToneOf(OptionalColor toolbar, OptionalColor frame) {
  if (toolbar.isSet) {
    return IsDark(toolbar.color) ? Tone::kDark : Tone::kLight;
  }
  if (frame.isSet) {
    return IsDark(frame.color) ? Tone::kDark : Tone::kLight;
  }
  return Tone::kUnknown;
}

}  // namespace fixedge::theme_tone
