// Unit tests for cyberpunk_skin.h.
#include <cstdio>
#include "../../src/theme/cyberpunk_skin.h"

using namespace edgeplus::cyberpunk_skin;
using edgeplus::theme_tone::IsDark;
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
  Check(ToneOfSelectedFill(0xff123456) == Tone::kDark,
        "a fill the skin did not choose reads as dark, the skin's default");
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
