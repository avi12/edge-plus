// Unit tests for remix_arguments.h.
#include <cstdio>
#include <string>
#include "../../src/theme/remix_arguments.h"

using edgeplus::remix_arguments::Outcome;
using edgeplus::remix_arguments::WithDarkmode;

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

std::string Wrap(const std::string& call) {
  return "(function() { try { " + call +
         "); } catch(e) { console.error('Execute failed:', e); } })();";
}

void RunChecks() {
  const std::string darkQuoted =
      Wrap("applyColorWheelThemeV2(\"\",\"\",\"true\",\"\",\"\"");
  const auto toLight = WithDarkmode(darkQuoted, false);
  Check(toLight.outcome == Outcome::kRewritten, "a quoted true is rewritten");
  Check(toLight.script ==
            Wrap("applyColorWheelThemeV2(\"\",\"\",\"false\",\"\",\"\""),
        "to a quoted false, the rest untouched");
  const auto toDark = WithDarkmode(toLight.script, true);
  Check(toDark.script == darkQuoted, "and back");
  Check(WithDarkmode(darkQuoted, true).outcome == Outcome::kUnchanged,
        "already right");

  const auto bare =
      WithDarkmode(Wrap("__edgeRemixApplyTheme(1, 2, false, 4, 5"), true);
  Check(bare.script == Wrap("__edgeRemixApplyTheme(1, 2, true, 4, 5"),
        "a bare boolean with spaces keeps its spacing");
  const auto number =
      WithDarkmode(Wrap("__edgeRemixApplyTheme(1,2,1,4,5"), false);
  Check(number.script == Wrap("__edgeRemixApplyTheme(1,2,0,4,5"), "a number");
  const auto single =
      WithDarkmode(Wrap("applyColorWheelThemeV2('a','b','1','c','d'"), false);
  Check(single.script == Wrap("applyColorWheelThemeV2('a','b','0','c','d'"),
        "a single-quoted number");

  const auto commas = WithDarkmode(
      Wrap("applyColorWheelThemeV2(\"a,b)\",[1,2],\"true\",\"\",\"\""), false);
  Check(commas.script ==
            Wrap("applyColorWheelThemeV2(\"a,b)\",[1,2],\"false\",\"\",\"\""),
        "commas inside quotes and brackets do not split");
  const auto escaped = WithDarkmode(
      Wrap("applyColorWheelThemeV2(\"a\\\",\",\"\",\"true\",\"\",\"\""), false);
  Check(escaped.outcome == Outcome::kRewritten, "an escaped quote");

  Check(WithDarkmode(Wrap("hideGTA6Videos("), true).outcome ==
            Outcome::kNotThemeCall,
        "another remix is left alone");
  Check(
      WithDarkmode(Wrap("myapplyColorWheelThemeV2(1,2,true"), false).outcome ==
          Outcome::kNotThemeCall,
      "a longer name is not the function");
  Check(WithDarkmode(Wrap("applyColorWheelThemeV2(\"\",\"\""), true).outcome ==
            Outcome::kUnknownShape,
        "too few arguments");
  Check(WithDarkmode(Wrap("applyColorWheelThemeV2(1,2,\"dark\",4,5"), true)
                .outcome == Outcome::kUnknownShape,
        "an unknown word");
  Check(WithDarkmode("", true).outcome == Outcome::kNotThemeCall, "empty");
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
