// skin_tone.h — the tone the Cyberpunk skin draws in right now: the theme
// pack's, recorded each time hooks.h reads its colours, else Windows' light or
// dark. The one place the tab frames, titles and omnibox frame ask. UI thread.
#pragma once
#include "theme/cyberpunk_skin.h"
#include "theme/theme.h"
#include "theme/theme_tone.h"

namespace edgeplus::skin_tone {

// Unknown until a theme pack's colours are read, and for a theme that colours
// neither its toolbar nor its frame.
inline constinit theme_tone::Tone g_themeTone = theme_tone::Tone::kUnknown;

inline theme_tone::Tone Current() {
  // Edge's own theme has no pack, so the last pack's tone no longer holds.
  const bool hasThemeTone = g_themeTone != theme_tone::Tone::kUnknown &&
                            !theme::g_isDefaultThemeApplied;
  if (hasThemeTone) {
    return g_themeTone;
  }
  return cyberpunk_skin::SkinToneFor(theme_tone::Tone::kUnknown,
                                     theme::IsOsDarkMode());
}

}  // namespace edgeplus::skin_tone
