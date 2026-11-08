// Unit tests for cyberpunk_skin.h.
#include <cstdio>
#include "../../src/theme/cyberpunk_skin.h"

using namespace edgeplus::cyberpunk_skin;
using edgeplus::theme_tone::Argb;
using edgeplus::theme_tone::ContrastRatio;
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

// The 2077 themes' rails and idle titles.
constexpr Argb kDarkRail = 0xff05080c;
constexpr Argb kDarkIdleTeal = 0xff00a6a6;
constexpr Argb kLightRail = 0xffd5e4f0;
constexpr Argb kLightIdleInk = 0xff2a4a66;

void RunToneChecks() {
  Check(SkinToneFor(Tone::kUnknown, true) == Tone::kDark &&
            SkinToneFor(Tone::kUnknown, false) == Tone::kLight,
        "with no theme tone the skin follows Windows' light or dark");
  Check(SkinToneFor(Tone::kLight, true) == Tone::kLight &&
            SkinToneFor(Tone::kDark, false) == Tone::kDark,
        "a theme's own tone wins over Windows'");
}

void RunSelectedTabChecks() {
  const SelectedTab dark = SelectedTabFor(Tone::kDark);
  const SelectedTab light = SelectedTabFor(Tone::kLight);
  Check(dark.frame == 0xfff72944 && dark.ink == 0xff56feff,
        "dark: boxed in the kit's red, titled in its cyan");
  Check(light.frame == 0xffc8102e && light.ink == 0xff06182b,
        "light: boxed in the deeper red, titled in ink, never cyan");
  Check(SelectedTabFor(Tone::kUnknown).frame == dark.frame,
        "a tone left unknown is taken as dark");
  Check(ContrastRatio(dark.ink, kDarkRail) >= kReadableContrast &&
            ContrastRatio(light.ink, kLightRail) >= kReadableContrast,
        "the selected title reads on the rail it has no fill over");
  Check(kFrameThicknessDip == 2, "the frame is the kit's 2px stroke");
  Check(IdleLineFor(Tone::kDark) == 0xff007979 &&
            IdleLineFor(Tone::kLight) == 0xff547a99,
        "the idle line is the kit's teal in dark and slate in light");
}

void RunIdleAndHoverChecks() {
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
  Check(
      darkHover != kDarkRail && darkHover != SelectedTabFor(Tone::kDark).frame,
      "a hover is a wash: neither the bare rail nor the accent");
  Check(ContrastRatio(darkHover, kDarkIdleTeal) >= kReadableContrast &&
            ContrastRatio(HoverFillFor(Tone::kLight, kLightRail, kLightIdleInk),
                          kLightIdleInk) >= kReadableContrast,
        "an idle title still reads on its hover");
}

void RunTitleFontChecks() {
  Check(TitleFontFamilyFor(true) == "Share Tech Mono",
        "the kit's mono when it is installed");
  Check(TitleFontFamilyFor(false) == "Consolas", "Consolas when it is not");
}

void RunChecks() {
  RunToneChecks();
  RunSelectedTabChecks();
  RunIdleAndHoverChecks();
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
