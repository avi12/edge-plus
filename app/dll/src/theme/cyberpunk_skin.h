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
#include <string_view>
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

// --- Frames (chrome/tabs/tab_frame.h) ---
// The kit carries structure with 2px strokes: the selected tab is framed in
// the frame colour (red in dark, ink in light) and every other tab in the idle
// line colour (the kit's "Lines" teal in dark, its "off" slate in light).
struct TabFrames {
  Argb selected = 0;
  Argb idle = 0;
};

inline constexpr TabFrames kDarkTabFrames{0xfff72944, 0xff007979};
inline constexpr TabFrames kLightTabFrames{0xff06182b, 0xff547a99};
// In DIPs, so it scales with the display as Edge's own tab stroke does.
inline constexpr int kFrameThicknessDip = 2;

[[nodiscard]] constexpr TabFrames FramesFor(theme_tone::Tone tone) noexcept {
  return tone == theme_tone::Tone::kLight ? kLightTabFrames : kDarkTabFrames;
}

// --- Field frame (chrome/toolbar/omnibox_frame.h) ---
// A text field's frame is the kit's idle line, the same as an idle tab's.
[[nodiscard]] constexpr Argb FieldFrameFor(theme_tone::Tone tone) noexcept {
  return FramesFor(tone).idle;
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

// The tone a selected tab's fill was chosen for: SelectedTabFor's inverse, so
// the frames follow the tone the fill already settled on. Unknown for a fill
// the skin did not choose (no theme pack coloured the tab).
[[nodiscard]] constexpr theme_tone::Tone ToneOfSelectedFill(
    Argb fill) noexcept {
  if (fill == kLightSelectedTab.fill) {
    return theme_tone::Tone::kLight;
  }
  if (fill == kDarkSelectedTab.fill) {
    return theme_tone::Tone::kDark;
  }
  return theme_tone::Tone::kUnknown;
}

}  // namespace edgeplus::cyberpunk_skin
