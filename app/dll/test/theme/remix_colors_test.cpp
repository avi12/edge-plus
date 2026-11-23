// Unit tests for remix_colors.h.
#include <cstdio>
#include <string>
#include "../../src/theme/remix_colors.h"

using namespace edgeplus::remix_colors;
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
  Pair pair;
  pair.light = Colour{210, 40, true};
  Check(FormatPair(pair) == "light=210,40;dark=", "an unset side is empty");
  Pair parsed;
  Check(ParsePair(FormatPair(pair), &parsed) && parsed == pair,
        "a pair survives its own text");
  pair.dark = Colour{0, 100, true};
  Check(ParsePair(FormatPair(pair), &parsed) && parsed == pair &&
            ColourFor(parsed, true).hue == 0 &&
            ColourFor(parsed, false).hue == 210,
        "both sides, picked by Edge's mode");
  Check(ParsePair("light=;dark=", &parsed) && !parsed.light.isSet &&
            !parsed.dark.isSet,
        "two unset sides");
  for (const char* bad :
       {"", "light=1,2", "dark=1,2;light=", "light=360,0;dark=",
        "light=0,101;dark=", "light=a,b;dark=", "light=1;dark=",
        "light=1,2;dark=1,2,3", "light=-1,0;dark="}) {
    Pair untouched = pair;
    Check(!ParsePair(bad, &untouched) && untouched == pair,
          "text that is not a pair changes nothing");
  }
  Pair fromUrl;
  Check(PairFromUrl("edge://blueprint/#edgeplus-remix=light=5,6;dark=7,8",
                    &fromUrl) &&
            fromUrl.dark.intensity == 8,
        "the pair is read from the address fragment");
  Check(!PairFromUrl("edge://blueprint/", &fromUrl), "no fragment, no pair");
  Check(PairFromUrl("edge://blueprint/#edgeplus-remix=light=5,6;dark=7,8;"
                    "preview=dark",
                    &fromUrl) &&
            fromUrl.dark.intensity == 8,
        "the pair is read before the preview field");
  Check(PreviewFromUrl("edge://blueprint/#edgeplus-remix=light=;dark=;"
                       "preview=dark") == Tone::kDark,
        "the previewed mode is read");
  Check(PreviewFromUrl("edge://blueprint/#edgeplus-remix=light=;dark=;"
                       "preview=light") == Tone::kLight,
        "light too");
  Check(PreviewFromUrl("edge://blueprint/#edgeplus-remix=light=;dark=;"
                       "preview=") == Tone::kUnknown,
        "an empty preview is none");
  Check(PreviewFromUrl("edge://blueprint/#edgeplus-remix=light=;dark=") ==
            Tone::kUnknown,
        "an older fragment has none");
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
