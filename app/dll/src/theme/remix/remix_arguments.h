// remix_arguments.h — rewrite an Appearance remix's call (remix_call.h): its
// colour set to the saved one for the target mode, and the call handed to the
// page's own helper (remix_page_helper.h) as a request for that mode, the
// helper picking the `darkmode` (docs/re/remix-theme.md). Pure: no Edge, no
// Win32.
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "theme/remix/remix_call.h"
#include "theme/remix/remix_colors.h"
#include "theme/remix/remix_page_helper.h"

namespace edgeplus::remix_arguments {

enum class Outcome {
  kNotThemeCall,  // not an Appearance remix's script: left alone
  kUnknownShape,  // a darkmode argument this does not understand: left alone
  kUnchanged,     // already what was asked
  kRewritten,
};

struct Result {
  Outcome outcome = Outcome::kNotThemeCall;
  std::string script;  // the new text, only when kRewritten
};

// `script` with its Appearance call handed to the helper as a request for the
// target mode `isTargetDark` (the helper picks the darkmode), and the helper
// installed before it, carrying `pair` for a later change of Edge's mode. A
// darkmode argument of a known spelling (or empty) is taken; another shape is
// left alone.
[[nodiscard]] inline Result WithPageAwareTheme(std::string_view script,
                                               bool isTargetDark,
                                               const remix_colors::Pair& pair) {
  const remix_call::ThemeCall call = remix_call::FindThemeCall(script);
  if (call.argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<remix_call::Span> arguments =
      remix_call::SplitArguments(script, call.argumentsAt);
  if (arguments.size() <= remix_call::kDarkmodeIndex) {
    return {Outcome::kUnknownShape, {}};
  }
  const std::string_view darkmode = remix_call::Unquoted(
      remix_call::TextOf(script, arguments[remix_call::kDarkmodeIndex]));
  if (!remix_call::IsKnownDarkmode(darkmode)) {
    return {Outcome::kUnknownShape, {}};
  }
  const size_t nameStart = call.argumentsAt - 1 - call.name.size();
  const size_t closingAt = arguments.back().end;
  std::string rewritten = "(";
  rewritten += remix_page_helper::kHelperScript;
  rewritten += ")(";
  rewritten += remix_page_helper::PairLiteral(pair);
  rewritten += ");\n";
  rewritten += script.substr(0, nameStart);
  rewritten += remix_page_helper::kRequestCall;
  rewritten += "\"";
  rewritten += call.name;
  rewritten += "\", ";
  rewritten += isTargetDark ? "true" : "false";
  for (const size_t index :
       {remix_call::kHueIndex, remix_call::kSaturationIndex,
        remix_call::kFontIndex, remix_call::kZoomIndex}) {
    rewritten += ", ";
    const bool isMissing = index >= arguments.size();
    if (isMissing) {
      rewritten += "undefined";
      continue;
    }
    rewritten += remix_call::TextOf(script, arguments[index]);
  }
  rewritten += ")";
  rewritten += script.substr(closingAt + 1);
  return {Outcome::kRewritten, std::move(rewritten)};
}

// `script` with the hue and saturation arguments of its Appearance call set to
// `hue` degrees and `saturation` percent, each in the quotes it already has.
[[nodiscard]] inline Result WithColour(std::string_view script, int hue,
                                       int saturation) {
  const size_t argumentsAt = remix_call::FindThemeCall(script).argumentsAt;
  if (argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<remix_call::Span> arguments =
      remix_call::SplitArguments(script, argumentsAt);
  if (arguments.size() <= remix_call::kDarkmodeIndex) {
    return {Outcome::kUnknownShape, {}};
  }
  const std::string hueText = std::to_string(hue);
  const std::string saturationText = std::to_string(saturation);
  std::string rewritten;
  size_t copiedTo = 0;
  for (const size_t index :
       {remix_call::kHueIndex, remix_call::kSaturationIndex}) {
    const remix_call::Span span = remix_call::Trimmed(script, arguments[index]);
    const std::string_view text = remix_call::TextOf(script, span);
    const std::string_view quote =
        remix_call::IsQuoted(text) ? text.substr(0, 1) : std::string_view();
    rewritten += script.substr(copiedTo, span.begin - copiedTo);
    rewritten += quote;
    rewritten += index == remix_call::kHueIndex ? hueText : saturationText;
    rewritten += quote;
    copiedTo = span.end;
  }
  rewritten += script.substr(copiedTo);
  if (rewritten == script) {
    return {Outcome::kUnchanged, {}};
  }
  return {Outcome::kRewritten, std::move(rewritten)};
}

}  // namespace edgeplus::remix_arguments
