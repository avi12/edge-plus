// Unit tests for remix_page_helper.h.
#include <cstdio>
#include <string>
#include "../../../src/theme/remix/remix_page_helper.h"

using edgeplus::remix_page_helper::kHelperScript;
using edgeplus::remix_page_helper::PairLiteral;

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

void RunPairLiteralChecks() {
  edgeplus::remix_colors::Pair pair;
  pair.light = {210, 40, true};
  Check(PairLiteral(pair) == "{light: [210, 40], dark: null}",
        "the pair goes to the page as numbers, an unset side as null");
  pair.dark = edgeplus::remix_colors::kNoColour;
  Check(PairLiteral(pair) == "{light: [210, 40], dark: [0, 0]}",
        "a mode with no colour goes as the centre: a push asks saturation 0");
}

void RunNoColourChecks() {
  const std::string helper(kHelperScript);
  Check(helper.find("helper.isNone = (want) => !(Number(want.saturation) > "
                    "0);") != std::string::npos,
        "the helper reads no colour as the template does: saturation not "
        "above 0");
  Check(helper.find("if (helper.none) {\n      await helper.wearOwnLook(") !=
                std::string::npos &&
            helper.find("helper.darkmode = \"false\";") != std::string::npos,
        "and wears the page's own look for it, with darkmode false and no "
        "check against the target");
}

}  // namespace

int main() {
  try {
    RunPairLiteralChecks();
    RunNoColourChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
