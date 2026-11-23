// remix_page_helper.h — the helper every Appearance apply goes through on the
// page, in the world remixes run in, as the script text that installs it, and
// the colour pair it carries (docs/re/remix-theme.md). Pure: strings only.
//
#pragma once
#include <string>
#include <string_view>
#include "theme/remix/remix_colors.h"

namespace edgeplus::remix_page_helper {

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
// - A request with no colour (saturation 0, the template's own test, which a
//   mode reset to remix_colors::kNoColour gets) wears the page's own look:
//   darkmode "false" whatever the target, the page's own darkness accepted,
//   nothing flipped or tinted. With no font or zoom either the template takes
//   its marks off the page, so that is not a stuck apply.
// - The outcome is left on the page for reading in DevTools:
//   `<html data-edgeplus-remix="target=dark;none=false;pageDark=false;
//   flipped=false;reapplied=false;forced=false;matched=true;applies=1;
//   skips=0">`.
//
// A function of the saved pair (`{light: [hue, intensity] or null, dark:
// ...}`) that installs the helper once per document (its functions are
// redefined each time, so a newer DLL's take over) and returns it.
inline constexpr std::string_view kHelperScript =
    R"js(function install(pair) {
  const helper = globalThis.__edgePlusRemix ||
      (globalThis.__edgePlusRemix = {learned: {}, applies: 0, skips: 0, none: false, flipped: false, reapplied: false, forced: false});
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
        ";none=" + helper.none + ";pageDark=" + helper.pageDark + ";flipped=" + helper.flipped + ";reapplied=" + helper.reapplied + ";forced=" + helper.forced +
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
  // No colour: the template tints only above saturation 0.
  helper.isNone = (want) => !(Number(want.saturation) > 0);
  // What an apply of `want` leaves: the template's marks, which an apply with
  // no colour, font or zoom takes off.
  helper.isDressedFor = (want) => helper.isNone(want) || helper.isThemed();
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
  // A request with no colour: the page's own look whatever the target (no
  // inversion, so a light page stays light in Edge's dark mode), the remix's
  // font and zoom kept; nothing to check against the target, so nothing
  // flipped, tinted or learned. Applied once more only when it never finished.
  helper.wearOwnLook = async (want, isSuperseded) => {
    helper.darkmode = "false";
    helper.mark("pending");
    helper.call(want, helper.isApplying() || !helper.isThemed());
    await helper.calm(8000);
    if (isSuperseded()) { return; }
    if (helper.isApplying()) {
      helper.reapplied = true;
      helper.call(want, true);
      await helper.calm(8000);
      if (isSuperseded()) { return; }
    }
    helper.worn = want;
    helper.mark(true);
  };
  // Applies `want` and checks the end state (themed, the target's colour and
  // darkness), applying once more when it is not. Gives way the moment a newer
  // request comes.
  helper.wear = async (want) => {
    const isSuperseded = () => helper.want !== want;
    helper.target = want.target;
    helper.none = helper.isNone(want);
    helper.flipped = false;
    helper.reapplied = false;
    helper.unforce();
    if (helper.none) {
      await helper.wearOwnLook(want, isSuperseded);
      return;
    }
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
    const isWorn = isRepeat && helper.worn === helper.want && helper.isDressedFor(want) && !helper.isApplying();
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

// The helper installed with `pair`, as an expression that returns it:
// `(kHelperScript)({light: [210, 40], dark: null})`.
[[nodiscard]] inline std::string InstalledHelper(
    const remix_colors::Pair& pair) {
  std::string expression = "(";
  expression += kHelperScript;
  expression += ")(";
  expression += PairLiteral(pair);
  expression += ")";
  return expression;
}

}  // namespace edgeplus::remix_page_helper
