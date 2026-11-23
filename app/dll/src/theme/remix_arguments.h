// remix_arguments.h — set the `darkmode` argument of an Appearance remix's call
// (docs/re/remix-theme.md). Edge runs a remix as the script text
// `(function() { try { NAME(arg0,arg1,...); } catch(e) {...} })();`; an
// Appearance remix's NAME is one of kThemeFunctions and its third argument is
// the light or dark baked in when the remix was made. Pure: no Edge, no Win32.
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edgeplus::remix_arguments {

// The functions an Appearance remix calls: the load-time wrapper's name and the
// template the editor stores its arguments under.
inline constexpr std::array<std::string_view, 2> kThemeFunctions = {
    "__edgeRemixApplyTheme", "applyColorWheelThemeV2"};

// (hue, saturation, darkmode, font, zoom). The editor's own "intensity" is
// the second one, in percent.
inline constexpr size_t kHueIndex = 0;
inline constexpr size_t kSaturationIndex = 1;
inline constexpr size_t kDarkmodeIndex = 2;

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

namespace detail {

// The spellings a darkmode argument takes, as {light, dark}.
struct Spelling {
  std::string_view light;
  std::string_view dark;
};
inline constexpr std::array<Spelling, 2> kSpellings = {
    Spelling{"false", "true"}, Spelling{"0", "1"}};

[[nodiscard]] constexpr bool IsIdentifierChar(char letter) noexcept {
  return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
         (letter >= '0' && letter <= '9') || letter == '_' || letter == '$';
}

[[nodiscard]] constexpr bool IsQuote(char letter) noexcept {
  return letter == '"' || letter == '\'' || letter == '`';
}

[[nodiscard]] constexpr bool IsSpace(char letter) noexcept {
  return letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n';
}

// Where the call `name(` opens in `script`: the index of the character after
// its parenthesis, or npos. The name must start a word.
[[nodiscard]] inline size_t FindCallArguments(std::string_view script,
                                              std::string_view name) {
  size_t from = 0;
  while (true) {
    const size_t nameStart = script.find(name, from);
    if (nameStart == std::string_view::npos) {
      return nameStart;
    }
    const size_t end = nameStart + name.size();
    const bool isWordStart =
        nameStart == 0 || !IsIdentifierChar(script[nameStart - 1]);
    const bool isCall = end < script.size() && script[end] == '(';
    if (isWordStart && isCall) {
      return end + 1;
    }
    from = nameStart + 1;
  }
}

struct Span {
  size_t begin = 0;
  size_t end = 0;  // one past
};

// The top-level arguments of a call whose parenthesis opened just before
// `start`: each one's span, with the quotes and nesting of its text respected.
// Empty when the call never closes.
[[nodiscard]] inline std::vector<Span> SplitArguments(std::string_view script,
                                                      size_t start) {
  std::vector<Span> arguments;
  size_t begin = start;
  int depth = 0;
  char quote = 0;
  for (size_t i = start; i < script.size(); i++) {
    const char letter = script[i];
    if (quote != 0) {
      if (letter == '\\') {
        i++;
      } else if (letter == quote) {
        quote = 0;
      }
      continue;
    }
    if (IsQuote(letter)) {
      quote = letter;
    } else if (letter == '(' || letter == '[' || letter == '{') {
      depth++;
    } else if (depth > 0 && (letter == ')' || letter == ']' || letter == '}')) {
      depth--;
    } else if (depth == 0 && letter == ',') {
      arguments.push_back({begin, i});
      begin = i + 1;
    } else if (depth == 0 && letter == ')') {
      arguments.push_back({begin, i});
      return arguments;
    }
  }
  return {};
}

// `argument` trimmed of white space.
[[nodiscard]] inline Span Trimmed(std::string_view script, Span argument) {
  while (argument.begin < argument.end && IsSpace(script[argument.begin])) {
    argument.begin++;
  }
  while (argument.end > argument.begin && IsSpace(script[argument.end - 1])) {
    argument.end--;
  }
  return argument;
}

}  // namespace detail

// `script` with the darkmode argument of its Appearance call set to `isDark`,
// written in the form it already has (`"true"`, `true` or `1`).
[[nodiscard]] inline Result WithDarkmode(std::string_view script, bool isDark) {
  size_t argumentsAt = std::string_view::npos;
  for (const std::string_view name : kThemeFunctions) {
    argumentsAt = detail::FindCallArguments(script, name);
    if (argumentsAt != std::string_view::npos) {
      break;
    }
  }
  if (argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<detail::Span> arguments =
      detail::SplitArguments(script, argumentsAt);
  if (arguments.size() <= kDarkmodeIndex) {
    return {Outcome::kUnknownShape, {}};
  }
  const detail::Span span = detail::Trimmed(script, arguments[kDarkmodeIndex]);
  std::string_view text = script.substr(span.begin, span.end - span.begin);
  std::string_view quote;
  const bool isQuoted = text.size() >= 2 && detail::IsQuote(text.front()) &&
                        text.back() == text.front();
  if (isQuoted) {
    quote = text.substr(0, 1);
    text = text.substr(1, text.size() - 2);
  }
  // The editor leaves the argument empty until its Dark button was used: the
  // page then reads it as light whatever Edge's mode is, so it is written out.
  const bool isEmptyQuoted = isQuoted && text.empty();
  if (isEmptyQuoted) {
    const detail::Spelling& spelling = detail::kSpellings.front();
    std::string rewritten(script.substr(0, span.begin));
    rewritten += quote;
    rewritten += isDark ? spelling.dark : spelling.light;
    rewritten += quote;
    rewritten += script.substr(span.end);
    return {Outcome::kRewritten, std::move(rewritten)};
  }
  for (const detail::Spelling& spelling : detail::kSpellings) {
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

// The template's `darkmode` does not mean "make the page dark": it inverts the
// lightness of every colour the page has. A page that already follows Edge's
// dark mode (prefers-color-scheme) is turned light by it, while a light page is
// turned dark. So the argument becomes, in the page, `String(wanted !== the
// page is dark)`: the page's own darkness is read at every apply from the
// background of its body (else its root), else from its dark color-scheme, with
// whatever an earlier apply left switched off for the read (the template's
// style elements, its `data-blueprint-cw-applied` marker and the background it
// writes inline on the body and root), so a page an earlier apply darkened is
// still read as its own light; not cached, so a change of Edge's mode is seen.
// Seen live: google.com dark + "true" -> body rgb(212,214,222).
inline constexpr std::string_view kPageIsDarkScript =
    R"js((()=>{const root=document.documentElement,body=document.body,nodes=[body,root].filter(Boolean),styles=[...document.querySelectorAll("#theme-blueprint-styles,#theme-blueprint-override,style[data-cross-origin]")],wasApplied=root.hasAttribute("data-blueprint-cw-applied"),inline=nodes.map(node=>[node.style.getPropertyValue("background-color"),node.style.getPropertyPriority("background-color")]);styles.forEach(style=>style.disabled=true);root.removeAttribute("data-blueprint-cw-applied");nodes.forEach(node=>node.style.removeProperty("background-color"));let isDark=null;for(const node of nodes){const parts=getComputedStyle(node).backgroundColor.match(/[\d.]+/g);if(parts&&(parts.length<4||+parts[3]>0)){isDark=.299*parts[0]+.587*parts[1]+.114*parts[2]<128;break}}if(isDark===null){isDark=matchMedia("(prefers-color-scheme: dark)").matches&&/dark/.test(getComputedStyle(root).colorScheme)}styles.forEach(style=>style.disabled=false);if(wasApplied){root.setAttribute("data-blueprint-cw-applied","")}nodes.forEach((node,index)=>{if(inline[index][0]){node.style.setProperty("background-color",inline[index][0],inline[index][1])}});return isDark})())js";

// `script` with the darkmode argument of its Appearance call replaced by the
// page-relative expression above for Edge's mode `isDark`. Any argument that is
// a known spelling (or empty) is replaced; another shape is left alone.
[[nodiscard]] inline Result WithPageAwareDarkmode(std::string_view script,
                                                  bool isDark) {
  size_t argumentsAt = std::string_view::npos;
  for (const std::string_view name : kThemeFunctions) {
    argumentsAt = detail::FindCallArguments(script, name);
    if (argumentsAt != std::string_view::npos) {
      break;
    }
  }
  if (argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<detail::Span> arguments =
      detail::SplitArguments(script, argumentsAt);
  if (arguments.size() <= kDarkmodeIndex) {
    return {Outcome::kUnknownShape, {}};
  }
  const detail::Span span = detail::Trimmed(script, arguments[kDarkmodeIndex]);
  std::string_view text = script.substr(span.begin, span.end - span.begin);
  const bool isQuoted = text.size() >= 2 && detail::IsQuote(text.front()) &&
                        text.back() == text.front();
  if (isQuoted) {
    text = text.substr(1, text.size() - 2);
  }
  bool isKnown = text.empty();
  for (const detail::Spelling& spelling : detail::kSpellings) {
    const bool isSpelling = text == spelling.light || text == spelling.dark;
    if (isSpelling) {
      isKnown = true;
    }
  }
  if (!isKnown) {
    return {Outcome::kUnknownShape, {}};
  }
  std::string rewritten(script.substr(0, span.begin));
  rewritten += "String(";
  rewritten += isDark ? "true" : "false";
  rewritten += "!==";
  rewritten += kPageIsDarkScript;
  rewritten += ")";
  rewritten += script.substr(span.end);
  return {Outcome::kRewritten, std::move(rewritten)};
}

// `script` with the hue and saturation arguments of its Appearance call set to
// `hue` degrees and `saturation` percent, each in the quotes it already has.
[[nodiscard]] inline Result WithColour(std::string_view script, int hue,
                                       int saturation) {
  size_t argumentsAt = std::string_view::npos;
  for (const std::string_view name : kThemeFunctions) {
    argumentsAt = detail::FindCallArguments(script, name);
    if (argumentsAt != std::string_view::npos) {
      break;
    }
  }
  if (argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<detail::Span> arguments =
      detail::SplitArguments(script, argumentsAt);
  if (arguments.size() <= kDarkmodeIndex) {
    return {Outcome::kUnknownShape, {}};
  }
  const std::string hueText = std::to_string(hue);
  const std::string saturationText = std::to_string(saturation);
  std::string rewritten;
  size_t copiedTo = 0;
  for (const size_t index : {kHueIndex, kSaturationIndex}) {
    const detail::Span span = detail::Trimmed(script, arguments[index]);
    std::string_view quote;
    const bool isQuoted = span.end - span.begin >= 2 &&
                          detail::IsQuote(script[span.begin]) &&
                          script[span.end - 1] == script[span.begin];
    if (isQuoted) {
      quote = script.substr(span.begin, 1);
    }
    rewritten += script.substr(copiedTo, span.begin - copiedTo);
    rewritten += quote;
    rewritten += index == kHueIndex ? hueText : saturationText;
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
