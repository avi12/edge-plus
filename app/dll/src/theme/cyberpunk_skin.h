// cyberpunk_skin.h — the Cyberpunk skin's selected tab, from the Cyberpunk UX
// kit: a red plate (the kit's tab fill) with ink that stays readable on it,
// pure black on the dark theme's red and white on the light theme's deeper red
// (the kit measured both at 5.4:1 and 5.9:1). The plate's cut corner is
// tab_shape.h's; the skin's switch is tab_shape::g_isCyberpunkSkinOn. Pure.
#pragma once
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

}  // namespace edgeplus::cyberpunk_skin
