// Unit tests for cyberpunk_skin.h.
#include <cstdio>
#include "../../src/theme/cyberpunk_skin.h"

using namespace edgeplus::cyberpunk_skin;
using edgeplus::theme_tone::Argb;
using edgeplus::theme_tone::ContrastRatio;
using edgeplus::theme_tone::IsDark;
using edgeplus::theme_tone::kReadableContrast;
using edgeplus::theme_tone::OptionalColor;
using edgeplus::theme_tone::Tone;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

// --- Frames ---
void RunFrameChecks() {
  const TabFrames dark = FramesFor(Tone::kDark);
  const TabFrames light = FramesFor(Tone::kLight);
  Check(dark.selected == 0xfff72944 && dark.idle == 0xff007979,
        "a dark theme frames the selected tab red and the others teal");
  Check(light.selected == 0xff06182b && light.idle == 0xff547a99,
        "a light theme frames the selected tab in ink and the others slate");
  Check(FramesFor(Tone::kUnknown).selected == dark.selected,
        "a theme of no tone is framed as dark");
  Check(ToneOfSelectedFill(SelectedTabFor(Tone::kLight).fill) == Tone::kLight,
        "the light fill reads back as light");
  Check(ToneOfSelectedFill(SelectedTabFor(Tone::kDark).fill) == Tone::kDark,
        "the dark fill reads back as dark");
  Check(ToneOfSelectedFill(0xff123456) == Tone::kUnknown,
        "a fill the skin did not choose has no tone of its own");
  Check(SkinToneFor(Tone::kUnknown, true) == Tone::kDark &&
            SkinToneFor(Tone::kUnknown, false) == Tone::kLight,
        "with no theme tone the skin follows Windows' light or dark");
  Check(SkinToneFor(Tone::kLight, true) == Tone::kLight &&
            SkinToneFor(Tone::kDark, false) == Tone::kDark,
        "a theme's own tone wins over Windows'");
  Check(kFrameThicknessDip == 2, "the frame is the kit's 2px stroke");
  Check(FieldFrameFor(Tone::kDark) == 0xff007979 &&
            FieldFrameFor(Tone::kLight) == 0xff547a99,
        "a text field is framed in the idle line colour");
}

// --- Title font ---
void RunTitleFontChecks() {
  Check(TitleFontFamilyFor(true) == "Share Tech Mono",
        "the kit's mono when it is installed");
  Check(TitleFontFamilyFor(false) == "Consolas", "Consolas when it is not");
}

void RunChecks() {
  const TabColors dark = SelectedTabFor(Tone::kDark);
  const TabColors light = SelectedTabFor(Tone::kLight);
  const TabColors unknown = SelectedTabFor(Tone::kUnknown);
  Check(dark.fill == 0xfff72944 && dark.ink == 0xff000000,
        "a dark theme gets the kit's red with black ink");
  Check(light.fill == 0xffc8102e && light.ink == 0xffffffff,
        "a light theme gets the deeper red with white ink");
  Check(unknown.fill == dark.fill && unknown.ink == dark.ink,
        "a theme of no tone is taken as dark");
  Check(IsDark(dark.ink) && !IsDark(light.ink),
        "the ink contrasts the way the tab's own ink rule reads it");
  Check(ContrastRatio(dark.fill, dark.ink) >= kReadableContrast &&
            ContrastRatio(light.fill, light.ink) >= kReadableContrast,
        "the selected title reads on its plate in both tones");

  // The 2077 themes' rails and titles.
  constexpr Argb kDarkRail = 0xff05080c;
  constexpr Argb kDarkIdleTeal = 0xff00a6a6;
  constexpr Argb kLightRail = 0xffd5e4f0;
  constexpr Argb kLightIdleInk = 0xff2a4a66;
  const OptionalColor darkIdle = IdleLabelFor({true, kDarkIdleTeal}, kDarkRail);
  Check(darkIdle.isSet && darkIdle.color == kDarkIdleTeal,
        "a readable idle colour titles the idle tabs");
  Check(IdleLabelFor({true, kLightIdleInk}, kLightRail).isSet,
        "the light theme's idle ink reads on its rail");
  Check(!IdleLabelFor({true, 0xff007979}, 0xff000000).isSet,
        "the kit's own teal is too dim to title a tab: Edge's colour stays");
  Check(!IdleLabelFor({}, kDarkRail).isSet,
        "a theme with no idle colour keeps Edge's titles");

  Check(HoverPercentFor(Tone::kDark) == kDarkHoverPercent &&
            HoverPercentFor(Tone::kLight) == kLightHoverPercent &&
            HoverPercentFor(Tone::kUnknown) == kDarkHoverPercent,
        "the hover share follows the tone, no tone taken as dark");
  Check(HoverFillFor(Tone::kDark, 0xff000000, kDarkIdleTeal) == 0xff001a1a,
        "the dark hover lands on the kit's teal wash over black");
  const Argb darkHover = HoverFillFor(Tone::kDark, kDarkRail, kDarkIdleTeal);
  Check(darkHover != kDarkRail && darkHover != dark.fill,
        "a hover is a wash: neither the bare rail nor the selected red");
  Check(ContrastRatio(darkHover, kDarkIdleTeal) >= kReadableContrast &&
            ContrastRatio(HoverFillFor(Tone::kLight, kLightRail, kLightIdleInk),
                          kLightIdleInk) >= kReadableContrast,
        "an idle title still reads on its hover");

  RunFrameChecks();
  RunTitleFontChecks();
}

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
