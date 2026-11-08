// cyberpunk_skin.h — the Cyberpunk skin's tabs, from the Cyberpunk UX kit's
// tab control (controls.css .c-tabs): plain rectangles in a mono row, the
// selected one boxed in the kit's accent with its "on" colour for text, the
// others bare in their idle colour. What a theme cannot say itself: Edge
// derives every vertical-tab colour from the theme's toolbar and paints every
// tab title in the theme's tab_text (offsets.h VTABCOLOR_*). The rail, toolbar
// and frame are the theme's; the square outline is tab_shape.h's; the switch is
// tab_shape::g_isCyberpunkSkinOn. Pure.
//
// - The selected tab: no fill of its own (the rail shows through), a 2px frame
//   in the accent (red in dark, the deeper red in light) and its title in the
//   "on" colour (cyan in dark; ink in light, where cyan never carries text).
// - Idle tabs: no frame, titled in the theme's tab_background_text, as Edge's
//   horizontal idle tabs are, while it reads on the rail (IdleLabelFor).
// - A hovered tab: a wash of the idle-title colour over the rail, the kit's
//   hover, never the accent (HoverFillFor).
#pragma once
#include <string_view>
#include "theme/theme_tone.h"

namespace edgeplus::cyberpunk_skin {

using theme_tone::Argb;
using theme_tone::OptionalColor;
using theme_tone::Tone;

struct SelectedTab {
  Argb frame = 0;
  Argb ink = 0;
};

inline constexpr SelectedTab kDarkSelectedTab{0xfff72944, 0xff56feff};
inline constexpr SelectedTab kLightSelectedTab{0xffc8102e, 0xff06182b};

// The kit's idle line, its "Lines" teal in dark and its "off" slate in light:
// what frames a text field.
inline constexpr Argb kDarkIdleLine = 0xff007979;
inline constexpr Argb kLightIdleLine = 0xff547a99;

// In DIPs, so it scales with the display as Edge's own tab stroke does.
inline constexpr int kFrameThicknessDip = 2;

// The kit's hover is its secondary teal #007979 at 22% over black in dark, and
// its ink at 8% in light. The wash here is of the theme's idle-title colour:
// as text that teal must be brighter than the kit's (#007979 is under 4.5:1 on
// black), so the dark share is smaller to land on the same wash (#00a6a6 at
// 16% is the kit's #001b1b).
inline constexpr uint32_t kDarkHoverPercent = 16;
inline constexpr uint32_t kLightHoverPercent = 8;

// The skin's tone: the theme's own, else Windows' light or dark (a default
// theme, or one that colours neither its toolbar nor its frame), so the skin
// flips with the system either way.
[[nodiscard]] constexpr Tone SkinToneFor(Tone themeTone,
                                         bool isSystemDark) noexcept {
  if (themeTone != Tone::kUnknown) {
    return themeTone;
  }
  return isSystemDark ? Tone::kDark : Tone::kLight;
}

// A tone left unknown is taken as dark: the kit's neon register.
[[nodiscard]] constexpr SelectedTab SelectedTabFor(Tone tone) noexcept {
  return tone == Tone::kLight ? kLightSelectedTab : kDarkSelectedTab;
}

[[nodiscard]] constexpr Argb IdleLineFor(Tone tone) noexcept {
  return tone == Tone::kLight ? kLightIdleLine : kDarkIdleLine;
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

// --- Title font (chrome/tabs/tab_font.h) ---
// The kit's mono, Share Tech Mono, when Windows has it installed; else
// Consolas, the monospace every Windows ships.
inline constexpr std::string_view kTitleFontFamily = "Share Tech Mono";
inline constexpr std::string_view kTitleFontFallback = "Consolas";

[[nodiscard]] constexpr std::string_view TitleFontFamilyFor(
    bool isKitFontInstalled) noexcept {
  return isKitFontInstalled ? kTitleFontFamily : kTitleFontFallback;
}

}  // namespace edgeplus::cyberpunk_skin
