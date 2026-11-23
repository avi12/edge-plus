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
  kUnchanged,     // already what Edge's mode says
  kRewritten,
};

struct Result {
  Outcome outcome = Outcome::kNotThemeCall;
  std::string script;  // the new text, only when kRewritten
};

// `script` with the darkmode argument of its Appearance call set to `isDark`,
// written in the form it already has (`"true"`, `true` or `1`).
[[nodiscard]] inline Result WithDarkmode(std::string_view script, bool isDark) {
  const size_t argumentsAt = remix_call::FindThemeCall(script).argumentsAt;
  if (argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<remix_call::Span> arguments =
      remix_call::SplitArguments(script, argumentsAt);
  if (arguments.size() <= remix_call::kDarkmodeIndex) {
    return {Outcome::kUnknownShape, {}};
  }
  const remix_call::Span span =
      remix_call::Trimmed(script, arguments[remix_call::kDarkmodeIndex]);
  std::string_view text = script.substr(span.begin, span.end - span.begin);
  std::string_view quote;
  const bool isQuoted = text.size() >= 2 && remix_call::IsQuote(text.front()) &&
                        text.back() == text.front();
  if (isQuoted) {
    quote = text.substr(0, 1);
    text = text.substr(1, text.size() - 2);
  }
  // The editor leaves the argument empty until its Dark button was used: the
  // page then reads it as light whatever Edge's mode is, so it is written out.
  const bool isEmptyQuoted = isQuoted && text.empty();
  if (isEmptyQuoted) {
    const remix_call::Spelling& spelling = remix_call::kSpellings.front();
    std::string rewritten(script.substr(0, span.begin));
    rewritten += quote;
    rewritten += isDark ? spelling.dark : spelling.light;
    rewritten += quote;
    rewritten += script.substr(span.end);
    return {Outcome::kRewritten, std::move(rewritten)};
  }
  for (const remix_call::Spelling& spelling : remix_call::kSpellings) {
    const bool isLight = text == spelling.light;
    const bool isDarkSpelled = text == spelling.dark;
    if (!isLight && !isDarkSpelled) {
      continue;
    }
    if (isDarkSpelled == isDark) {
      return {Outcome::kUnchanged, {}};
    }
    std::string rewritten(script.substr(0, span.begin));
    rewritten += quote;
    rewritten += isDark ? spelling.dark : spelling.light;
    rewritten += quote;
    rewritten += script.substr(span.end);
    return {Outcome::kRewritten, std::move(rewritten)};
  }
  return {Outcome::kUnknownShape, {}};
}

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
  const remix_call::Span span =
      remix_call::Trimmed(script, arguments[remix_call::kDarkmodeIndex]);
  std::string_view text = script.substr(span.begin, span.end - span.begin);
  const bool isQuoted = text.size() >= 2 && remix_call::IsQuote(text.front()) &&
                        text.back() == text.front();
  if (isQuoted) {
    text = text.substr(1, text.size() - 2);
  }
  bool isKnown = text.empty();
  for (const remix_call::Spelling& spelling : remix_call::kSpellings) {
    const bool isSpelling = text == spelling.light || text == spelling.dark;
    if (isSpelling) {
      isKnown = true;
    }
  }
  if (!isKnown) {
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
    const remix_call::Span argument =
        remix_call::Trimmed(script, arguments[index]);
    rewritten += script.substr(argument.begin, argument.end - argument.begin);
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
    std::string_view quote;
    const bool isQuoted = span.end - span.begin >= 2 &&
                          remix_call::IsQuote(script[span.begin]) &&
                          script[span.end - 1] == script[span.begin];
    if (isQuoted) {
      quote = script.substr(span.begin, 1);
    }
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
