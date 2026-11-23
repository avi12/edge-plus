// Unit tests for remix_call.h.
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include "../../../src/theme/remix/remix_call.h"

using edgeplus::remix_call::FindThemeCall;
using edgeplus::remix_call::IsKnownDarkmode;
using edgeplus::remix_call::IsQuoted;
using edgeplus::remix_call::Span;
using edgeplus::remix_call::SplitArguments;
using edgeplus::remix_call::TextOf;
using edgeplus::remix_call::ThemeCall;
using edgeplus::remix_call::Unquoted;

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

// The trimmed text of each argument of the script's Appearance call.
std::vector<std::string> ArgumentsOf(std::string_view script) {
  const ThemeCall call = FindThemeCall(script);
  std::vector<std::string> texts;
  for (const Span& argument : SplitArguments(script, call.argumentsAt)) {
    texts.emplace_back(TextOf(script, argument));
  }
  return texts;
}

void RunFindChecks() {
  const std::string editor =
      Wrap("applyColorWheelThemeV2(\"\",\"\",\"true\",\"\",\"\"");
  Check(FindThemeCall(editor).name == "applyColorWheelThemeV2",
        "the editor's call is found by the template's name");
  Check(FindThemeCall(Wrap("__edgeRemixApplyTheme(1, 2, false, 4, 5")).name ==
            "__edgeRemixApplyTheme",
        "and the load-time wrapper's");
  Check(FindThemeCall(Wrap("hideGTA6Videos(")).argumentsAt ==
            std::string_view::npos,
        "another remix is no Appearance call");
  Check(FindThemeCall(Wrap("myapplyColorWheelThemeV2(1,2,true")).argumentsAt ==
            std::string_view::npos,
        "a longer name is not the function");
  Check(FindThemeCall("function applyColorWheelThemeV2(hue, saturation) {}")
                .argumentsAt == std::string_view::npos,
        "the template's own declaration is not a call");
  Check(FindThemeCall("").argumentsAt == std::string_view::npos, "empty");
}

void RunSplitChecks() {
  Check(ArgumentsOf(Wrap("__edgeRemixApplyTheme(1, 2, false, 4, 5")) ==
            std::vector<std::string>{"1", "2", "false", "4", "5"},
        "arguments are split at top-level commas and trimmed");
  Check(ArgumentsOf(Wrap("applyColorWheelThemeV2('a','b','1','c','d'"))[2] ==
            "'1'",
        "a single-quoted argument keeps its quotes");
  Check(ArgumentsOf(
            Wrap("applyColorWheelThemeV2(\"a,b)\",[1,2],\"true\",\"\",\"\"")) ==
            std::vector<std::string>{"\"a,b)\"", "[1,2]", "\"true\"", "\"\"",
                                     "\"\""},
        "commas inside quotes and brackets do not split");
  Check(ArgumentsOf(Wrap(
            "applyColorWheelThemeV2(\"a\\\",\",\"\",\"true\",\"\",\"\""))[2] ==
            "\"true\"",
        "an escaped quote does not end its string");
  Check(ArgumentsOf(Wrap("applyColorWheelThemeV2(\"\",\"\"")).size() == 2,
        "a short call has its few arguments");
  const std::string unclosed = "applyColorWheelThemeV2(1, 2, true";
  Check(SplitArguments(unclosed, FindThemeCall(unclosed).argumentsAt).empty(),
        "a call that never closes has none");
}

void RunQuoteChecks() {
  Check(IsQuoted("\"true\"") && IsQuoted("'1'") && IsQuoted("``") &&
            !IsQuoted("true") && !IsQuoted("\"") && !IsQuoted("\"a'"),
        "a string literal has one quote at each end");
  Check(Unquoted("\"true\"") == "true" && Unquoted("1") == "1" &&
            Unquoted("\"\"").empty(),
        "unquoting takes the quotes off, and only them");
  Check(IsKnownDarkmode("true") && IsKnownDarkmode("false") &&
            IsKnownDarkmode("1") && IsKnownDarkmode("0") &&
            IsKnownDarkmode("") && !IsKnownDarkmode("dark") &&
            !IsKnownDarkmode("maybe"),
        "a darkmode is a spelling of light or dark, or empty");
}

}  // namespace

int main() {
  try {
    RunFindChecks();
    RunSplitChecks();
    RunQuoteChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
