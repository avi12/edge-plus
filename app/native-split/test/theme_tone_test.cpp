// Unit tests for the pure theme-tone decision (theme_tone.h). No Edge/Win32
// deps. Run via test\run.cmd.
#include <cstdio>
#include "../src/theme_tone.h"

using namespace fixedge::theme_tone;

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
  Check(Halfway(kBlack, kWhite) == 0xff7f7f7f,
        "halfway black to white is grey");
  Check(Halfway(kNavy, kNavy) == kNavy, "halfway to itself changes nothing");
  Check(Halfway(0x8000ff00, kBlack) == 0xff007f00,
        "halfway is opaque and per channel");
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
