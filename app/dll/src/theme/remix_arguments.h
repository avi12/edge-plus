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
// turned dark. Seen live: google.com dark + "true" -> body rgb(212,214,222).
// So the call is wrapped in a helper of the page's own (kPrepareScript before
// it, kVerifyScript after it) and its darkmode argument names the helper's
// choice, `target-is-dark XOR page-is-dark`:
// - The first guess of the page's own darkness samples what is rendered at a
//   few points of the viewport (each point's element, up to the first opaque
//   background; else the canvas, read off a probe painted `Canvas`, which
//   follows the page's color-scheme and prefers-color-scheme), with what an earlier
//   apply left behind switched off for the read (the template's style elements,
//   its marker attribute, the background it writes on body and root). Inline
//   colours it rewrote on elements cannot be told from the page's, so a guess
//   once confirmed (below) is kept per prefers-color-scheme for the document.
// - After the template's asynchronous apply has settled the page is sampled
//   again. When its darkness is not the target's, the apply is made once more
//   with the opposite darkmode, through the template's own re-apply; when that
//   misses too (a see-through page in the other prefers-color-scheme, a dark
//   page with coloured backgrounds), root and body are tinted to the target.
// - The outcome is left on the page for reading in DevTools:
//   `<html data-edgeplus-remix="target=dark;pageDark=false;flipped=false;
//   forced=false;matched=true">`.
inline constexpr std::string_view kDarkmodeExpression =
    "globalThis.__edgePlusRemix.darkmode";

// A function of `isTargetDark`, called before the theme call.
inline constexpr std::string_view kPrepareScript = R"js((isTargetDark) => {
  const helper = globalThis.__edgePlusRemix || (globalThis.__edgePlusRemix = {learned: {}});
  helper.generation = (helper.generation || 0) + 1;
  helper.target = isTargetDark;
  helper.flipped = false;
  helper.scheme = () => (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
  // A colour's luma, 0 to 255, or null for a see-through one.
  helper.lumaOf = (colour) => {
    const text = colour || "";
    const numbers = text.slice(text.indexOf("(") + 1).replace(/^[a-z][\w-]*\s+/i, "").match(/-?[\d.]+%?/g);
    if (!numbers || numbers.length < 3) { return null; }
    const value = (number) => (number.endsWith("%") ? parseFloat(number) / 100 : parseFloat(number));
    const alpha = numbers.length > 3 ? value(numbers[3]) : 1;
    if (alpha < 0.5) { return null; }
    if (/^ok(lch|lab)\(/i.test(text)) { return value(numbers[0]) * 255; }
    const scale = /^color\(/i.test(text) ? 255 : 1;
    return (0.299 * value(numbers[0]) + 0.587 * value(numbers[1]) + 0.114 * value(numbers[2])) * scale;
  };
  // The canvas as the page's colour scheme paints it (its color-scheme
  // property or meta, with prefers-color-scheme choosing among them).
  helper.canvasLuma = () => {
    const probe = document.createElement("div");
    probe.style.cssText = "position: absolute; width: 0; height: 0; visibility: hidden; background-color: Canvas;";
    document.documentElement.append(probe);
    const luma = helper.lumaOf(getComputedStyle(probe).backgroundColor);
    probe.remove();
    return luma === null ? 255 : luma;
  };
  // The luma of what is rendered at a few points of the viewport: each point's
  // element, up to the first opaque background, else the canvas.
  helper.sample = () => {
    const points = [[0.5, 0.5], [0.15, 0.15], [0.85, 0.15], [0.15, 0.85], [0.85, 0.85], [0.5, 0.2], [0.5, 0.8]];
    return points.map(([across, down]) => {
      let node = document.elementFromPoint(innerWidth * across, innerHeight * down);
      let luma = null;
      while (node && luma === null) {
        luma = helper.lumaOf(getComputedStyle(node).backgroundColor);
        node = node.parentElement;
      }
      return luma === null ? helper.canvasLuma() : luma;
    });
  };
  const countBelow = (lumas, limit) => lumas.filter((luma) => luma < limit).length;
  helper.isDarkGuess = (lumas) => countBelow(lumas, 128) * 2 > lumas.length;
  // Clearly the target: most points well on its side (a mid tone is neither).
  helper.isTargetShown = (lumas) => {
    const matching = helper.target ? countBelow(lumas, 100) : lumas.length - countBelow(lumas, 156);
    return matching * 2 > lumas.length;
  };  helper.sampleOwn = () => {
    const root = document.documentElement;
    const nodes = [document.body, root].filter(Boolean);
    const styles = [...document.querySelectorAll("#theme-blueprint-styles, #theme-blueprint-override, style[data-cross-origin]")];
    const wasApplied = root.hasAttribute("data-blueprint-cw-applied");
    const inline = nodes.map((node) => [node.style.getPropertyValue("background-color"), node.style.getPropertyPriority("background-color")]);
    styles.forEach((style) => { style.disabled = true; });
    root.removeAttribute("data-blueprint-cw-applied");
    nodes.forEach((node) => node.style.removeProperty("background-color"));
    try {
      return helper.isDarkGuess(helper.sample());
    } finally {
      styles.forEach((style) => { style.disabled = false; });
      if (wasApplied) { root.setAttribute("data-blueprint-cw-applied", ""); }
      nodes.forEach((node, index) => {
        if (inline[index][0]) { node.style.setProperty("background-color", inline[index][0], inline[index][1]); }
      });
    }
  };
  helper.mark = (isMatched) => {
    document.documentElement.dataset.edgeplusRemix = "target=" + (helper.target ? "dark" : "light") +
        ";pageDark=" + helper.pageDark + ";flipped=" + helper.flipped + ";forced=" + helper.forced +
        ";matched=" + isMatched;
  };
  // The last resort, when neither darkmode gives the target (the template
  // paints a see-through page by prefers-color-scheme whatever its darkmode,
  // and lifts a dark page's coloured backgrounds either way): the page's root
  // and body get a tint of the hue in the target's lightness and the target's
  // colour scheme. A style element marked as the template's own cross-origin
  // copy, which its stylesheet rewrite and its observer leave alone; the
  // background it wrote inline on root and body is taken off for it.
  const forceSelector = 'style[data-cross-origin="edgeplus-remix-force"]';
  document.querySelectorAll(forceSelector).forEach((style) => style.remove());
  helper.forced = false;
  helper.force = () => {
    const hue = Number((globalThis.__blueprintParams || {}).hue) || 0;
    const tint = helper.target ? "hsl(" + hue + ", 35%, 12%)" : "hsl(" + hue + ", 30%, 93%)";
    const style = document.createElement("style");
    style.dataset.crossOrigin = "edgeplus-remix-force";
    style.textContent = "html, body { background-color: " + tint + " !important; }" +
        " html[data-blueprint-cw-applied][data-edgeplus-remix], html { color-scheme: " +
        (helper.target ? "dark" : "light") + " !important; }";
    (document.head || document.documentElement).append(style);
    [document.documentElement, document.body].filter(Boolean)
        .forEach((node) => node.style.removeProperty("background-color"));
    helper.forced = true;
  };
  helper.choose = (isPageDark) => {
    helper.pageDark = isPageDark;
    helper.darkmode = String(helper.target !== isPageDark);
  };
  helper.verify = () => {
    const generation = helper.generation;
    const startedAt = Date.now();
    let calmBeats = 0;
    const check = () => {
      if (helper.generation !== generation) { return; }
      const library = globalThis.__blueprintThemeLib__ || {};
      const isApplying = library.__observerState ? library.__observerState.isApplying === true : false;
      calmBeats = isApplying ? 0 : calmBeats + 1;
      const isSettled = calmBeats >= 3 || Date.now() - startedAt > 5000;
      if (!isSettled) { setTimeout(check, 100); return; }
      const isMatched = helper.isTargetShown(helper.sample());
      const params = globalThis.__blueprintParams;
      const canFlip = !isMatched && !helper.flipped && params && typeof library.applyFromBlueprintParams === "function";
      if (canFlip) {
        helper.flipped = true;
        helper.choose(!helper.pageDark);
        params.darkmode = helper.darkmode;
        helper.mark("pending");
        library.applyFromBlueprintParams();
        calmBeats = 0;
        setTimeout(check, 150);
        return;
      }
      if (!isMatched && !helper.forced) {
        helper.force();
        helper.mark(helper.isTargetShown(helper.sample()));
        return;
      }
      if (isMatched && !helper.forced) { helper.learned[helper.scheme()] = helper.pageDark; }
      helper.mark(isMatched);
    };
    setTimeout(check, 150);
  };
  const learned = helper.learned[helper.scheme()];
  helper.choose(typeof learned === "boolean" ? learned : helper.sampleOwn());
  helper.mark("pending");
})js";

inline constexpr std::string_view kVerifyScript =
    "globalThis.__edgePlusRemix && globalThis.__edgePlusRemix.verify();";

// `script` with the darkmode argument of its Appearance call replaced by the
// helper's choice for the target mode `isTargetDark`, and the helper around
// it. Any argument that is a known spelling (or empty) is replaced; another
// shape is left alone.
[[nodiscard]] inline Result WithPageAwareTheme(std::string_view script,
                                               bool isTargetDark) {
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
  std::string rewritten = "(";
  rewritten += kPrepareScript;
  rewritten += ")(";
  rewritten += isTargetDark ? "true" : "false";
  rewritten += ");\n";
  rewritten += script.substr(0, span.begin);
  rewritten += kDarkmodeExpression;
  rewritten += script.substr(span.end);
  rewritten += "\n";
  rewritten += kVerifyScript;
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
