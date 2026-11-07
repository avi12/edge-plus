// Unit tests for theme_tone.h.
#include <cstdio>
#include "../../src/theme/theme_tone.h"

using namespace edgeplus::theme_tone;

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

constexpr Argb kBlack = 0xff000000;
constexpr Argb kWhite = 0xffffffff;
constexpr Argb kNavy = 0xff28283c;       // a dark toolbar
constexpr Argb kCream = 0xfffffaf0;      // a light toolbar
constexpr Argb kPureGreen = 0xff00ff00;  // bright: green carries the most luma
constexpr Argb kPureBlue = 0xff0000ff;   // dim: blue carries the least

void RunChecks() {
  Check(IsDark(kBlack) && !IsDark(kWhite), "black is dark, white is light");
  Check(IsDark(kNavy) && !IsDark(kCream),
        "a navy toolbar is dark, cream light");
  Check(!IsDark(kPureGreen) && IsDark(kPureBlue),
        "channels are weighted: green is light, blue is dark");
  Check(Luma(kWhite) == 255 && Luma(kBlack) == 0, "luma spans 0..255");
  Check(IsDark(0x00000000) == IsDark(kBlack),
        "alpha does not change the verdict");

  const OptionalColor unset;
  const OptionalColor darkToolbar{true, kNavy};
  const OptionalColor lightToolbar{true, kCream};
  const OptionalColor darkFrame{true, kBlack};
  const OptionalColor lightFrame{true, kWhite};
  Check(ToneOf(darkToolbar, lightFrame) == Tone::kDark,
        "the toolbar decides over the frame (dark)");
  Check(ToneOf(lightToolbar, darkFrame) == Tone::kLight,
        "the toolbar decides over the frame (light)");
  Check(ToneOf(unset, darkFrame) == Tone::kDark,
        "no toolbar color: the frame decides");
  Check(ToneOf(unset, lightFrame) == Tone::kLight,
        "no toolbar color: a light frame is light");
  Check(ToneOf(unset, unset) == Tone::kUnknown, "neither set: no verdict");
  Check(Blend(kBlack, kWhite, 50) == 0xff7f7f7f, "half of the way is grey");
  Check(Blend(kNavy, kNavy, 30) == kNavy,
        "blending toward itself changes nothing");
  Check(Blend(kNavy, kWhite, 0) == kNavy && Blend(kNavy, kWhite, 100) == kWhite,
        "zero keeps the colour, a hundred takes the other");
  Check(Blend(0x8000ff00, kBlack, 50) == 0xff007f00,
        "the blend is opaque and per channel");

  const double blackOnWhite = ContrastRatio(kBlack, kWhite);
  Check(blackOnWhite > 20.99 && blackOnWhite < 21.01,
        "black on white is the full 21:1");
  Check(ContrastRatio(kNavy, kNavy) == 1.0, "a colour on itself is 1:1");
  Check(ContrastRatio(kBlack, kWhite) == ContrastRatio(kWhite, kBlack),
        "the ratio does not depend on the order");
  // The Cyberpunk UX kit's measurements: its red reads 5.36:1 under black ink,
  // and its #007979 teal falls short as text on black.
  const double kitRedUnderBlack = ContrastRatio(0xfff72944, kBlack);
  Check(kitRedUnderBlack > 5.3 && kitRedUnderBlack < 5.4,
        "the kit's red under black ink measures as the kit does");
  Check(ContrastRatio(0xff007979, kBlack) < kReadableContrast,
        "the kit's teal is too dim for text on black");
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
