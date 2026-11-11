// Unit tests for cyberpunk_palette.h: the kit's colour rules, as numbers.
#include <cstdio>
#include "../../src/theme/cyberpunk_palette.h"

using namespace edgeplus::cyberpunk_palette;
using edgeplus::theme_tone::Argb;
using edgeplus::theme_tone::ContrastRatio;
using edgeplus::theme_tone::kReadableContrast;
using edgeplus::theme_tone::Luma;
using edgeplus::theme_tone::Tone;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

void Check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

constexpr Tone kTones[] = {Tone::kDark, Tone::kLight};
constexpr Role kTextRoles[] = {Role::kForeground, Role::kForegroundMuted,
                               Role::kOn, Role::kLabel, Role::kAlertText};

void RunTextReadsOnThePanel() {
  for (const Tone tone : kTones) {
    const Argb panel = ColourOf(Role::kPanel, tone);
    for (const Role role : kTextRoles) {
      Check(ContrastRatio(ColourOf(role, tone), panel) >= kReadableContrast,
            "every text role reads at 4.5:1 on the panel, in both tones");
    }
    Check(ContrastRatio(ColourOf(Role::kInk, tone),
                        ColourOf(Role::kOnFill, tone)) >= kReadableContrast,
          "ink reads on a lit fill");
    Check(ContrastRatio(ColourOf(Role::kTooltipForeground, tone),
                        ColourOf(Role::kTooltipBackground, tone)) >=
              kReadableContrast,
          "a tooltip's text reads on its background");
  }
}

void RunKitRules() {
  Check(ColourOf(Role::kOnFill, Tone::kDark) ==
                ColourOf(Role::kOnFill, Tone::kLight) &&
            ColourOf(Role::kOnFill, Tone::kDark) == 0xff56feff,
        "the lit fill is the kit's cyan in both tones");
  Check(Luma(ColourOf(Role::kOn, Tone::kLight)) < 64,
        "in light, cyan never carries text: the 'on' colour is ink");
  Check(ColourOf(Role::kOn, Tone::kDark) == 0xff56feff,
        "in dark, the 'on' colour is the kit's cyan");
  for (const Tone tone : kTones) {
    for (const Role role : kTextRoles) {
      Check(ColourOf(role, tone) != ColourOf(Role::kAccent, tone),
            "red never carries text");
    }
    Check(ColourOf(Role::kTooltipBackground, tone) !=
              ColourOf(Role::kOnFill, tone),
          "a tooltip is never a cyan fill");
    Check(ColourOf(Role::kPanel, tone) != ColourOf(Role::kOnFill, tone),
          "a floating panel is never a cyan fill");
  }
  Check(ColourOf(Role::kAccent, Tone::kDark) == 0xfff72944 &&
            ColourOf(Role::kAccent, Tone::kLight) == 0xffc8102e,
        "the accent is the kit's red, the deeper one in light");
}

void RunWashAndUnknown() {
  for (const Tone tone : kTones) {
    const Argb wash = ColourOf(Role::kOnFillWash, tone);
    Check((wash >> 24) == 0xff, "the switched-on track is opaque");
    Check(wash != ColourOf(Role::kPanel, tone) &&
              wash != ColourOf(Role::kOnFill, tone),
          "the wash sits between the panel and the lit fill");
  }
  Check(PaletteFor(Tone::kUnknown).panel == kDark.panel,
        "a tone left unknown is taken as dark");
  Check(ColourOf(Role::kNone, Tone::kDark) == 0, "no role gives no colour");
}

}  // namespace

int main() {
  try {
    RunTextReadsOnThePanel();
    RunKitRules();
    RunWashAndUnknown();
  } catch (const CheckFailed&) {
    return 1;
  }
  std::printf("cyberpunk_palette_test: all checks passed\n");
  return 0;
}
