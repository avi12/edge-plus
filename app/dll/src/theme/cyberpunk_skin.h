// cyberpunk_skin.h — the Cyberpunk skin's selected tab, from the Cyberpunk UX
// kit: a red plate (the kit's tab fill) with ink that stays readable on it,
// pure black on the dark theme's red and white on the light theme's deeper red
// (the kit measured both at 5.4:1 and 5.9:1). The plate's cut corner is
// tab_shape.h's; the skin's switch is tab_shape::g_isCyberpunkSkinOn. Pure.
#pragma once
#include <string_view>
#include "theme/theme_tone.h"

namespace edgeplus::cyberpunk_skin {

using theme_tone::Argb;

struct TabColors {
  Argb fill = 0;
  Argb ink = 0;
};

inline constexpr TabColors kDarkSelectedTab{0xfff72944, 0xff000000};
inline constexpr TabColors kLightSelectedTab{0xffc8102e, 0xffffffff};

// A theme that colours neither its toolbar nor its frame is taken as dark: the
// kit's neon register.
[[nodiscard]] constexpr TabColors SelectedTabFor(
    theme_tone::Tone tone) noexcept {
  return tone == theme_tone::Tone::kLight ? kLightSelectedTab
                                          : kDarkSelectedTab;
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
// the frames follow the tone the fill already settled on.
[[nodiscard]] constexpr theme_tone::Tone ToneOfSelectedFill(
    Argb fill) noexcept {
  return fill == kLightSelectedTab.fill ? theme_tone::Tone::kLight
                                        : theme_tone::Tone::kDark;
}

}  // namespace edgeplus::cyberpunk_skin
