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
#include "theme/remix_colors.h"

namespace edgeplus::remix_arguments {

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

}  // namespace detail

// `script` with the darkmode argument of its Appearance call set to `isDark`,
// written in the form it already has (`"true"`, `true` or `1`).
[[nodiscard]] inline Result WithDarkmode(std::string_view script, bool isDark) {
  const size_t argumentsAt = detail::FindThemeCall(script).argumentsAt;
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
// So every apply goes through a helper of the page's own, in the world the
// remix runs in (kHelperScript), which picks the darkmode as
// `target-is-dark XOR page-is-dark`:
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
//   with the opposite darkmode; when that misses too (a see-through page in
//   the other prefers-color-scheme, a dark page with coloured backgrounds),
//   root and body are tinted to the target.
// - The applies are serialized, so two appliers (Edge's own call, the DLL's
//   push on a mode flip, the page's own listener) never abort each other
//   half way. The template remembers the key of its last apply before its
//   asynchronous work and aborts the apply in flight on a new one, so an
//   aborted apply could leave the page bare while a later apply of the same
//   key returned at once: an apply in flight is let finish first, the key is
//   forgotten whenever the page is not themed, the latest request wins, and a
//   request for exactly what the page already wears makes no apply.
// - The outcome is left on the page for reading in DevTools:
//   `<html data-edgeplus-remix="target=dark;pageDark=false;flipped=false;
//   reapplied=false;forced=false;matched=true;applies=1;skips=0">`.
//
// A function of the saved pair (`{light: [hue, intensity] or null, dark:
// ...}`) that installs the helper once per document (its functions are
// redefined each time, so a newer DLL's take over) and returns it.
inline constexpr std::string_view kHelperScript =
    R"js(function install(pair) {
  const helper = globalThis.__edgePlusRemix ||
      (globalThis.__edgePlusRemix = {learned: {}, applies: 0, skips: 0, flipped: false, reapplied: false, forced: false});
  helper.pair = pair;
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
  helper.mark = (status) => {
    helper.status = status;
    document.documentElement.dataset.edgeplusRemix = "target=" + (helper.target ? "dark" : "light") +
        ";pageDark=" + helper.pageDark + ";flipped=" + helper.flipped + ";reapplied=" + helper.reapplied + ";forced=" + helper.forced +
        ";matched=" + status + ";applies=" + helper.applies + ";skips=" + helper.skips;
  };
  helper.library = () => globalThis.__blueprintThemeLib__ || {};
  // The template's apply in flight: its own flag, up from the moment it starts
  // until its asynchronous work is done (and briefly while its observer fixes
  // an element the page restyled).
  helper.isApplying = () => {
    const observer = helper.library().__observerState;
    return observer ? observer.isApplying === true : false;
  };
  // The marks the template leaves on a page it themes.
  helper.isThemed = () => document.documentElement.hasAttribute("data-blueprint-cw-applied") &&
      !!document.getElementById("theme-blueprint-styles");
  // The last resort, when neither darkmode gives the target (the template
  // paints a see-through page by prefers-color-scheme whatever its darkmode,
  // and lifts a dark page's coloured backgrounds either way): the page's root
  // and body get a tint of the hue in the target's lightness and the target's
  // colour scheme. A style element marked as the template's own cross-origin
  // copy, which its stylesheet rewrite and its observer leave alone; the
  // background it wrote inline on root and body is taken off for it.
  const forceSelector = 'style[data-cross-origin="edgeplus-remix-force"]';
  helper.unforce = () => {
    document.querySelectorAll(forceSelector).forEach((style) => style.remove());
    helper.forced = false;
  };
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
  // Whether `one` reads nearer the target than `other`: darker on average for
  // a dark target, lighter for a light one.
  helper.isNearer = (one, other) => {
    const mean = (lumas) => lumas.reduce((sum, luma) => sum + luma, 0) / lumas.length;
    return helper.target ? mean(one) < mean(other) : mean(one) > mean(other);
  };
  helper.choose = (isPageDark) => {
    helper.pageDark = isPageDark;
    helper.darkmode = String(helper.target !== isPageDark);
  };
  // Resolves once the template has nothing in flight (two quiet beats, the
  // page loaded), or after `limitMs` of the page being shown: a hidden page's
  // timers barely run, so only the time it is shown counts. The caller reads
  // the outcome off the page.
  helper.calm = (limitMs) => new Promise((resolve) => {
    let quietBeats = 0;
    let shownMs = 0;
    let lastAt = Date.now();
    const check = () => {
      const now = Date.now();
      if (document.visibilityState === "visible") { shownMs += now - lastAt; }
      lastAt = now;
      quietBeats = helper.isApplying() ? 0 : quietBeats + 1;
      const isCalm = quietBeats >= 2 && document.readyState === "complete";
      if (isCalm || shownMs > limitMs) { resolve(); return; }
      setTimeout(check, 100);
    };
    setTimeout(check, 100);
  });
  // One call of the template, `want` in the chosen darkmode; with its key
  // forgotten when asked, so it does not skip a key it believes is worn.
  helper.call = (want, shouldForgetKey) => {
    const state = helper.library().__themeState;
    if (shouldForgetKey && state) { state.lastApplyKey = ""; }
    helper.applies++;
    globalThis[want.name](want.hue, want.saturation, helper.darkmode, want.font, want.zoom);
  };
  // Applies `want` and checks the end state (themed, the target's colour and
  // darkness), applying once more when it is not. Gives way the moment a newer
  // request comes.
  helper.wear = async (want) => {
    const isSuperseded = () => helper.want !== want;
    helper.target = want.target;
    helper.flipped = false;
    helper.reapplied = false;
    helper.unforce();
    const learned = helper.learned[helper.scheme()];
    // Mid-apply the page is half restored, so the last guess stands then.
    const hasGuess = typeof helper.pageDark === "boolean";
    if (typeof learned === "boolean") {
      helper.choose(learned);
    } else if (helper.isApplying() && hasGuess) {
      helper.choose(helper.pageDark);
    } else {
      helper.choose(helper.sampleOwn());
    }
    helper.mark("pending");
    // An apply still in flight after the wait is one the template never
    // finished: this one replaces it.
    helper.call(want, helper.isApplying() || !helper.isThemed());
    await helper.calm(8000);
    if (isSuperseded()) { return; }
    // The template left the page bare, or never finished: once more.
    const isStuck = helper.isApplying() || !helper.isThemed();
    if (isStuck) {
      helper.reapplied = true;
      helper.call(want, true);
      await helper.calm(8000);
      if (isSuperseded()) { return; }
    }
    let lumas = helper.sample();
    let isMatched = helper.isThemed() && helper.isTargetShown(lumas);
    if (!isMatched) {
      // A page fading its colours in is read again a moment later.
      await new Promise((resolve) => setTimeout(resolve, 600));
      if (isSuperseded()) { return; }
      lumas = helper.sample();
      isMatched = helper.isThemed() && helper.isTargetShown(lumas);
    }
    if (!isMatched) {
      const firstLumas = lumas;
      helper.flipped = true;
      helper.choose(!helper.pageDark);
      helper.mark("pending");
      helper.call(want, !helper.isThemed());
      await helper.calm(8000);
      if (isSuperseded()) { return; }
      lumas = helper.sample();
      isMatched = helper.isThemed() && helper.isTargetShown(lumas);
      // Neither reaches the target (the template lifts a page's colours to mid
      // tones either way): the one nearer it is kept, not the last tried.
      const isFurther = !isMatched && helper.isNearer(firstLumas, lumas);
      if (isFurther) {
        helper.choose(!helper.pageDark);
        helper.call(want, !helper.isThemed());
        await helper.calm(8000);
        if (isSuperseded()) { return; }
      }
    }
    if (isMatched) {
      helper.learned[helper.scheme()] = helper.pageDark;
    } else {
      helper.force();
      isMatched = helper.isTargetShown(helper.sample());
    }
    helper.worn = want;
    helper.mark(isMatched);
  };
  // One request at a time, the latest winning; the apply in flight (Edge's,
  // an earlier one or the template's own observer) is let finish first.
  helper.pump = async () => {
    helper.isPumping = true;
    try {
      while (helper.want !== helper.handled) {
        const want = helper.want;
        if (helper.isApplying()) {
          await helper.calm(1500);
          if (helper.want !== want) { continue; }
        }
        helper.handled = want;
        try {
          await helper.wear(want);
        } catch (error) {
          helper.error = String(error);
        }
      }
    } finally {
      helper.isPumping = false;
    }
  };
  helper.isSame = (one, other) => !!one && !!other && one.name === other.name && one.target === other.target &&
      Math.round(Number(one.hue)) === Math.round(Number(other.hue)) &&
      Math.round(Number(one.saturation)) === Math.round(Number(other.saturation)) &&
      String(one.font ?? "") === String(other.font ?? "") && String(one.zoom ?? "") === String(other.zoom ?? "");
  // Every apply goes through here: Edge's own call (rewritten by the DLL), the
  // DLL's push when Edge's mode flips and the page's own listener. A request
  // for what is already being applied, or already worn, under the page's
  // current prefers-color-scheme (its own colours, which the darkmode is
  // chosen against) makes no apply.
  helper.request = (name, isTargetDark, hue, saturation, font, zoom) => {
    const want = {name, target: isTargetDark, hue: String(hue), saturation: String(saturation), font, zoom};
    const scheme = helper.scheme();
    const isRepeat = helper.isSame(want, helper.want) && helper.wantScheme === scheme;
    const isWorn = isRepeat && helper.worn === helper.want && helper.isThemed() && !helper.isApplying();
    if (isRepeat && (helper.isPumping || isWorn)) {
      helper.skips++;
      helper.mark(helper.status);
      return;
    }
    helper.want = want;
    helper.wantScheme = scheme;
    if (!helper.isPumping) { helper.pump(); }
  };
  // Edge's mode flipping under an applied remix: the remix again in that mode's
  // saved colour (else the last call's), with the last call's font and zoom.
  helper.follow = (isTargetDark, name) => {
    const last = helper.want || globalThis.__blueprintParams;
    const canApply = !!last && typeof globalThis[name] === "function";
    if (!canApply) { return; }
    const colour = helper.pair ? helper.pair[isTargetDark ? "dark" : "light"] : null;
    helper.request(name, isTargetDark, colour ? colour[0] : last.hue, colour ? colour[1] : last.saturation,
        last.font, last.zoom);
  };
  // The DLL's push (FollowModeScript), which carries Edge's own mode: the one
  // authority on the target.
  helper.push = (isTargetDark, name) => {
    helper.follow(isTargetDark, helper.want ? helper.want.name : name);
  };
  // The page's prefers-color-scheme flipping (a quarter second after the last
  // flip; a hidden tab hears it once shown) changes its own colours under the
  // remix: the same request again, its darkmode chosen anew.
  helper.onScheme = () => {
    const want = helper.want;
    if (!want) { return; }
    helper.request(want.name, want.target, want.hue, want.saturation, want.font, want.zoom);
  };
  if (!helper.isFollowingScheme) {
    helper.isFollowingScheme = true;
    matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => {
      const live = globalThis.__edgePlusRemix;
      clearTimeout(live.schemeTimer);
      live.schemeTimer = setTimeout(() => live.onScheme(), 250);
    });
  }
  return helper;
})js";

// How the helper is reached: an Appearance call `NAME(hue, saturation,
// darkmode, font, zoom)` becomes `kRequestCall"NAME", isTargetDark, hue,
// saturation, font, zoom)`, the darkmode left to the helper.
inline constexpr std::string_view kRequestCall =
    "globalThis.__edgePlusRemix.request(";

// FollowModeScript's pieces, around kHelperScript: only a page wearing an
// Appearance remix (applied by any build of the DLL, or by Edge alone) is
// asked.
inline constexpr std::string_view kFollowModeOpening = R"js((() => {
  if (!globalThis.__blueprintParams) { return; }
  ()js";
inline constexpr std::string_view kFollowModePush = ").push(";
inline constexpr std::string_view kFollowModeClosing = R"js();
})();)js";

// The pair as the helper takes it: `{light: [210, 40], dark: null}`.
[[nodiscard]] inline std::string PairLiteral(const remix_colors::Pair& pair) {
  const auto side = [](const remix_colors::Colour& colour) {
    if (!colour.isSet) {
      return std::string("null");
    }
    return "[" + std::to_string(colour.hue) + ", " +
           std::to_string(colour.intensity) + "]";
  };
  return "{light: " + side(pair.light) + ", dark: " + side(pair.dark) + "}";
}

// `script` with its Appearance call handed to the helper as a request for the
// target mode `isTargetDark` (the helper picks the darkmode), and the helper
// installed before it, carrying `pair` for a later change of Edge's mode. A
// darkmode argument of a known spelling (or empty) is taken; another shape is
// left alone.
[[nodiscard]] inline Result WithPageAwareTheme(std::string_view script,
                                               bool isTargetDark,
                                               const remix_colors::Pair& pair) {
  const detail::ThemeCall call = detail::FindThemeCall(script);
  if (call.argumentsAt == std::string_view::npos) {
    return {Outcome::kNotThemeCall, {}};
  }
  const std::vector<detail::Span> arguments =
      detail::SplitArguments(script, call.argumentsAt);
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
  const size_t nameStart = call.argumentsAt - 1 - call.name.size();
  const size_t closingAt = arguments.back().end;
  std::string rewritten = "(";
  rewritten += kHelperScript;
  rewritten += ")(";
  rewritten += PairLiteral(pair);
  rewritten += ");\n";
  rewritten += script.substr(0, nameStart);
  rewritten += kRequestCall;
  rewritten += "\"";
  rewritten += call.name;
  rewritten += "\", ";
  rewritten += isTargetDark ? "true" : "false";
  for (const size_t index :
       {kHueIndex, kSaturationIndex, kFontIndex, kZoomIndex}) {
    rewritten += ", ";
    const bool isMissing = index >= arguments.size();
    if (isMissing) {
      rewritten += "undefined";
      continue;
    }
    const detail::Span argument = detail::Trimmed(script, arguments[index]);
    rewritten += script.substr(argument.begin, argument.end - argument.begin);
  }
  rewritten += ")";
  rewritten += script.substr(closingAt + 1);
  return {Outcome::kRewritten, std::move(rewritten)};
}

// The script that applies a tab's Appearance remix again for Edge's new mode
// `isTargetDark`, in `pair`'s colour for it, through the tab's helper.
[[nodiscard]] inline std::string FollowModeScript(
    bool isTargetDark, const remix_colors::Pair& pair) {
  std::string script(kFollowModeOpening);
  script += kHelperScript;
  script += ")(" + PairLiteral(pair);
  script += kFollowModePush;
  script += isTargetDark ? "true" : "false";
  script += ", \"";
  script += kTemplateFunction;
  script += "\"";
  script += kFollowModeClosing;
  return script;
}

// `script` with the hue and saturation arguments of its Appearance call set to
// `hue` degrees and `saturation` percent, each in the quotes it already has.
[[nodiscard]] inline Result WithColour(std::string_view script, int hue,
                                       int saturation) {
  const size_t argumentsAt = detail::FindThemeCall(script).argumentsAt;
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
