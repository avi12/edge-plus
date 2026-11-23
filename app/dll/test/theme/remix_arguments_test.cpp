// Unit tests for remix_arguments.h.
#include <cstdio>
#include <string>
#include "../../src/theme/remix_arguments.h"

using edgeplus::remix_arguments::FollowModeScript;
using edgeplus::remix_arguments::kHelperScript;
using edgeplus::remix_arguments::kRequestCall;
using edgeplus::remix_arguments::Outcome;
using edgeplus::remix_arguments::PairLiteral;
using edgeplus::remix_arguments::WithColour;
using edgeplus::remix_arguments::WithDarkmode;
using edgeplus::remix_arguments::WithPageAwareTheme;

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

void RunEmptyDarkmodeChecks() {
  const std::string empty =
      Wrap("applyColorWheelThemeV2(\"10\",\"20\",\"\",\"\",\"\"");
  const auto dark = WithDarkmode(empty, true);
  Check(dark.outcome == Outcome::kRewritten &&
            dark.script ==
                Wrap("applyColorWheelThemeV2(\"10\",\"20\",\"true\",\"\",\"\""),
        "an empty darkmode becomes true in dark");
  const auto light = WithDarkmode(empty, false);
  Check(
      light.outcome == Outcome::kRewritten &&
          light.script ==
              Wrap("applyColorWheelThemeV2(\"10\",\"20\",\"false\",\"\",\"\""),
      "an empty darkmode becomes false in light");
}

void RunColourChecks() {
  const std::string quoted =
      Wrap("applyColorWheelThemeV2(\"10\",\"20\",\"true\",\"\",\"\"");
  const auto coloured = WithColour(quoted, 210, 40);
  Check(
      coloured.outcome == Outcome::kRewritten &&
          coloured.script ==
              Wrap("applyColorWheelThemeV2(\"210\",\"40\",\"true\",\"\",\"\""),
      "hue and saturation are replaced inside their quotes");
  Check(WithColour(quoted, 10, 20).outcome == Outcome::kUnchanged,
        "the colour already there is left alone");
  const auto bare =
      WithColour(Wrap("__edgeRemixApplyTheme( 5 , 6 ,false,\"\",\"\""), 300, 0);
  Check(bare.outcome == Outcome::kRewritten &&
            bare.script ==
                Wrap("__edgeRemixApplyTheme( 300 , 0 ,false,\"\",\"\""),
        "unquoted numbers stay unquoted and keep their spacing");
  const auto empty = WithColour(
      Wrap("applyColorWheelThemeV2(\"\",\"\",\"0\",\"\",\"\""), 1, 2);
  Check(empty.outcome == Outcome::kRewritten &&
            empty.script ==
                Wrap("applyColorWheelThemeV2(\"1\",\"2\",\"0\",\"\",\"\""),
        "an empty colour argument is filled");
  Check(WithColour("unrelated()", 1, 2).outcome == Outcome::kNotThemeCall &&
            WithColour(Wrap("applyColorWheelThemeV2(1"), 1, 2).outcome ==
                Outcome::kUnknownShape,
        "another script, or a call too short, is left alone");
}

void RunChecks() {
  RunColourChecks();
  RunEmptyDarkmodeChecks();
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

const edgeplus::remix_colors::Pair kNoPair{};

void RunPageAwareChecks() {
  const std::string call =
      Wrap("applyColorWheelThemeV2(\"228\",\"20\",\"true\",\"\",\"\"");
  const auto dark = WithPageAwareTheme(call, true, kNoPair);
  const std::string expectedCall =
      Wrap(std::string(kRequestCall) +
           "\"applyColorWheelThemeV2\", true, \"228\", \"20\", \"\", \"\"");
  Check(dark.outcome == Outcome::kRewritten, "a theme call is rewritten");
  Check(dark.script.starts_with("(" + std::string(kHelperScript) +
                                ")({light: null, dark: null});\n"),
        "the helper is installed first, with the pair");
  Check(dark.script.ends_with(expectedCall),
        "the call becomes a request for the target mode, its darkmode left "
        "to the helper and the rest of Edge's script kept");
  const auto fromEmpty = WithPageAwareTheme(
      Wrap("applyColorWheelThemeV2(\"228\",\"20\",\"\",\"\",\"\""), true,
      kNoPair);
  Check(fromEmpty.script == dark.script, "an empty darkmode too");
  const auto light = WithPageAwareTheme(
      Wrap("__edgeRemixApplyTheme(1, 2, 1, 4, 5"), false, kNoPair);
  Check(light.script.ends_with(
            Wrap(std::string(kRequestCall) +
                 "\"__edgeRemixApplyTheme\", false, 1, 2, 4, 5")),
        "a light target asks for light, through the function Edge named");
  const auto withoutFontAndZoom = WithPageAwareTheme(
      Wrap("applyColorWheelThemeV2(1, 2, true"), true, kNoPair);
  Check(withoutFontAndZoom.script.find("true, 1, 2, undefined, undefined)") !=
            std::string::npos,
        "missing font and zoom are passed as undefined");
  Check(WithPageAwareTheme(Wrap("applyColorWheelThemeV2(1,2,maybe,4,5"), true,
                           kNoPair)
                .outcome == Outcome::kUnknownShape,
        "an unknown word is left alone");
  Check(WithPageAwareTheme(Wrap("hideGTA6Videos("), true, kNoPair).outcome ==
            Outcome::kNotThemeCall,
        "another remix is left alone");
}

void RunLoadShapeChecks() {
  // A stored remix applied as its page loads, and the template it calls.
  const std::string load =
      "(function(){try{applyColorWheelThemeV2(\"228\",\"20.5\",\"true\",\"\","
      "\"\");return true;}catch(e){return false;}})();";
  const auto coloured = WithColour(load, 120, 50);
  Check(
      coloured.script.find("applyColorWheelThemeV2(\"120\",\"50\",\"true\"") !=
          std::string::npos,
      "the load-time call takes the saved colour");
  const auto requested = WithPageAwareTheme(load, true, kNoPair);
  Check(requested.outcome == Outcome::kRewritten &&
            requested.script.ends_with(
                "(function(){try{" + std::string(kRequestCall) +
                "\"applyColorWheelThemeV2\", true, \"228\", \"20.5\", \"\", "
                "\"\");return true;}catch(e){return false;}})();"),
        "and becomes a request, still answering Edge with true");
  const std::string declaration =
      "function applyColorWheelThemeV2(hue, saturation, darkmode, font, zoom) "
      "{ window.__blueprintParams = {hue:hue}; }";
  Check(WithColour(declaration, 120, 50).outcome == Outcome::kNotThemeCall,
        "the template's own declaration is not a call");
  Check(WithPageAwareTheme(declaration, true, kNoPair).outcome ==
            Outcome::kNotThemeCall,
        "and is left alone");
}
void RunPairLiteralChecks() {
  edgeplus::remix_colors::Pair pair;
  pair.light = {210, 40, true};
  Check(PairLiteral(pair) == "{light: [210, 40], dark: null}",
        "the pair goes to the page as numbers, an unset side as null");
}

void RunFollowModeChecks() {
  edgeplus::remix_colors::Pair pair;
  pair.dark = {228, 20, true};
  const std::string script = FollowModeScript(true, pair);
  Check(script.starts_with(
            "(() => {\n  if (!globalThis.__blueprintParams) { return; }\n  (" +
            std::string(kHelperScript) + ")({light: null, dark: [228, 20]})"),
        "only a page wearing an Appearance remix installs the helper, with "
        "the saved pair");
  Check(script.ends_with(".push(true, \"applyColorWheelThemeV2\");\n})();"),
        "and pushes the target mode to it, as one self-contained call");
  Check(
      FollowModeScript(false, pair).find(".push(false, ") != std::string::npos,
      "a light flip pushes light");
}
}  // namespace

int main() {
  try {
    RunChecks();
    RunPageAwareChecks();
    RunPairLiteralChecks();
    RunLoadShapeChecks();
    RunFollowModeChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
