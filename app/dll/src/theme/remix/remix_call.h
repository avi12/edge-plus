// remix_call.h — an Appearance remix's call in the script Edge runs it as
// (docs/re/remix-theme.md): `(function() { try { NAME(arg0,arg1,...); }
// catch(e) {...} })();` for the editor's applies, `(function(){try{NAME(...);
// return true;}catch(e){return false;}})();` for a stored remix as its page
// loads. NAME is one of kThemeFunctions; the call is found and its top-level
// arguments split with their quotes and nesting respected. Pure: no Edge, no
// Win32.
#pragma once
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace edgeplus::remix_call {

// The functions an Appearance remix calls: the load-time wrapper's name and the
// template the editor stores its arguments under.
inline constexpr std::string_view kTemplateFunction = "applyColorWheelThemeV2";
inline constexpr std::array<std::string_view, 2> kThemeFunctions = {
    "__edgeRemixApplyTheme", kTemplateFunction};

// (hue, saturation, darkmode, font, zoom). The editor's own "intensity" is
// the second one, in percent.
inline constexpr size_t kHueIndex = 0;
inline constexpr size_t kSaturationIndex = 1;
inline constexpr size_t kDarkmodeIndex = 2;
inline constexpr size_t kFontIndex = 3;
inline constexpr size_t kZoomIndex = 4;

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
    // The template itself declares the function: `function NAME(hue, ...)`.
    const std::string_view before = script.substr(0, nameStart);
    const size_t lastWordEnd = before.find_last_not_of(" \t\r\n");
    const bool isDeclaration =
        lastWordEnd != std::string_view::npos &&
        before.substr(0, lastWordEnd + 1).ends_with("function");
    const bool isCallOfName = isWordStart && isCall && !isDeclaration;
    if (isCallOfName) {
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

// The first Appearance call in `script`: the function it names and where its
// arguments start (npos when there is none).
struct ThemeCall {
  std::string_view name;
  size_t argumentsAt = std::string_view::npos;
};

[[nodiscard]] inline ThemeCall FindThemeCall(std::string_view script) {
  for (const std::string_view name : kThemeFunctions) {
    const size_t argumentsAt = FindCallArguments(script, name);
    if (argumentsAt != std::string_view::npos) {
      return {name, argumentsAt};
    }
  }
  return {};
}

}  // namespace edgeplus::remix_call
