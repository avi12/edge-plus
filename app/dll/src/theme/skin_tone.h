// skin_tone.h — the tone the Cyberpunk skin draws in right now: the theme
// pack's, recorded each time hooks.h reads its colours, else Windows' light or
// dark. The one place the tab frames, titles, omnibox frame and the browser's
// colour roles ask. UI thread.
#pragma once
#include <windows.h>
#include "theme/cyberpunk_skin.h"
#include "theme/theme.h"
#include "theme/theme_tone.h"

namespace edgeplus::skin_tone {

// Unknown until a theme pack's colours are read, and for a theme that colours
// neither its toolbar nor its frame.
inline constinit theme_tone::Tone g_themeTone = theme_tone::Tone::kUnknown;

// The lParam of channel kForceSkinTone: which tone a script asks for.
enum class ForcedTone : LPARAM {
  kNone = 0,  // the theme's own, else Windows'
  kLight = 1,
  kDark = 2,
};

// A script's stand-in for the theme's tone (channel kForceSkinTone), so both
// tones can be checked without flipping Windows' mode. Unknown: not forced.
inline constinit theme_tone::Tone g_forcedTone = theme_tone::Tone::kUnknown;

// The colour provider asks for every colour of every view, so Windows' mode
// (a registry read) is taken at most once a second.
inline constexpr ULONGLONG kOsModeValidMilliseconds = 1000;
inline constinit ULONGLONG g_osModeReadAt = 0;
inline constinit bool g_isOsDark = false;

inline bool IsOsDark() {
  const ULONGLONG now = GetTickCount64();
  const bool isStale =
      g_osModeReadAt == 0 || now - g_osModeReadAt > kOsModeValidMilliseconds;
  if (isStale) {
    g_isOsDark = theme::IsOsDarkMode();
    g_osModeReadAt = now;
  }
  return g_isOsDark;
}

inline theme_tone::Tone Current() {
  const bool isForced = g_forcedTone != theme_tone::Tone::kUnknown;
  if (isForced) {
    return g_forcedTone;
  }
  // Edge's own theme has no pack, so the last pack's tone no longer holds.
  const bool hasThemeTone = g_themeTone != theme_tone::Tone::kUnknown &&
                            !theme::g_isDefaultThemeApplied;
  if (hasThemeTone) {
    return g_themeTone;
  }
  return cyberpunk_skin::SkinToneFor(theme_tone::Tone::kUnknown, IsOsDark());
}

}  // namespace edgeplus::skin_tone
