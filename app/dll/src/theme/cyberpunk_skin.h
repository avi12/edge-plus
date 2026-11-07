// cyberpunk_skin.h — the Cyberpunk skin's vertical tabs, from the Cyberpunk UX
// kit, for what a theme cannot say itself: Edge derives every vertical-tab
// colour from the theme's toolbar and paints every tab title in the theme's
// tab_text (offsets.h VTABCOLOR_*). The rail, toolbar and frame are the
// theme's. The plate's cut corner is tab_shape.h's; the skin's switch is
// tab_shape::g_isCyberpunkSkinOn. Pure.
//
// - The selected tab: a red plate (the kit's tab fill) with ink that stays
//   readable on it, pure black on the dark theme's red and white on the light
//   theme's deeper red (the kit measured both at 5.4:1 and 5.9:1).
// - Idle titles: the kit's rule is idle dim, current bright, so they take the
//   theme's tab_background_text, as Edge's horizontal idle tabs do, while it
//   reads on the rail (IdleLabelFor).
// - A hovered tab: a wash of the idle-title colour over the rail, the kit's
//   hover, never the selected red (HoverFillFor).
#pragma once
#include "theme/theme_tone.h"

namespace edgeplus::cyberpunk_skin {

using theme_tone::Argb;
using theme_tone::OptionalColor;
using theme_tone::Tone;

struct TabColors {
  Argb fill = 0;
  Argb ink = 0;
};

inline constexpr TabColors kDarkSelectedTab{0xfff72944, 0xff000000};
inline constexpr TabColors kLightSelectedTab{0xffc8102e, 0xffffffff};

// The kit's hover is its secondary teal #007979 at 22% over black in dark, and
// its ink at 8% in light. The wash here is of the theme's idle-title colour:
// as text that teal must be brighter than the kit's (#007979 is under 4.5:1 on
// black), so the dark share is smaller to land on the same wash (#00a6a6 at
// 16% is the kit's #001b1b).
inline constexpr uint32_t kDarkHoverPercent = 16;
inline constexpr uint32_t kLightHoverPercent = 8;

// A theme that colours neither its toolbar nor its frame is taken as dark: the
// kit's neon register.
[[nodiscard]] constexpr TabColors SelectedTabFor(Tone tone) noexcept {
  return tone == Tone::kLight ? kLightSelectedTab : kDarkSelectedTab;
}

[[nodiscard]] constexpr uint32_t HoverPercentFor(Tone tone) noexcept {
  return tone == Tone::kLight ? kLightHoverPercent : kDarkHoverPercent;
}

// The idle titles' colour, or unset to leave Edge's (every title in tab_text)
// when the theme names none or names one that would not read on the rail.
[[nodiscard]] inline OptionalColor IdleLabelFor(OptionalColor idleText,
                                                Argb rail) noexcept {
  const bool isReadable =
      idleText.isSet && theme_tone::ContrastRatio(idleText.color, rail) >=
                            theme_tone::kReadableContrast;
  return isReadable ? idleText : OptionalColor{};
}

[[nodiscard]] constexpr Argb HoverFillFor(Tone tone, Argb rail,
                                          Argb label) noexcept {
  return theme_tone::Blend(rail, label, HoverPercentFor(tone));
}

}  // namespace edgeplus::cyberpunk_skin
