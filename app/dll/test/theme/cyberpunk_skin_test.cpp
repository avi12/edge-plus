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
