// remix_selector.h — the Light | Dark colour selector of Create a remix's
// Appearance editor (docs/re/remix-theme.md). It needs no skin; with the
// skin on it takes the skin's roles (remix_skin.h).
//
// With the remix switch on, an Appearance remix wears one colour for Edge's
// light mode and another for its dark mode (remix_colors.h), as the Edge+ theme
// has a pair. The editor's "Dark" button, which only toggled the remix's own
// darkmode (now Edge's, remix_darkmode.h), is hidden and a Light | Dark
// selector takes its place: it says which mode the wheel is editing, and
// switching it moves the wheel to that mode's colour (the saved one when
// editing a stored remix, none yet in a new appearance until the user picks
// it). A row under it resets the selected mode's colour, or both, to no colour
// (the page's own look). Font and zoom stay shared. The page's own wheel is
// read from its slider's text and driven with pointer events (our isolated
// world cannot see the page's script objects, only the shared DOM). The pair
// goes back to the DLL in the page's address fragment.
//
// Its reset row is remix_selector_reset.h, its leave guard
// remix_selector_leave.h, and what it asks of the page's own world
// remix_wheel_bridge.h. Pure: strings only.
#pragma once
#include <string>
#include "find/web_text.h"
#include "theme/remix/pages/remix_selector_leave.h"
#include "theme/remix/pages/remix_selector_reset.h"
#include "theme/remix/pages/remix_wheel_bridge.h"
#include "theme/remix/remix_colors.h"

namespace edgeplus::remix_selector {

inline constexpr char kSelectorClass[] = "edgeplus-mode-pick";
inline constexpr char kSelectorStyleClass[] = "edgeplus-mode-style";
inline constexpr char kSelectorRowClass[] = "edgeplus-mode-row";
inline constexpr char kSelectorEdgeClass[] = "edgeplus-mode-edge";
inline constexpr char kSelectorPreviewClass[] = "edgeplus-mode-preview";
inline constexpr char kSelectorStatusClass[] = "edgeplus-mode-status";
inline constexpr char kSelectorStateKey[] = "__edgeplusRemixPick";
inline constexpr char kLightLabel[] = "Light";
inline constexpr char kDarkLabel[] = "Dark";
inline constexpr char kSelectorLabel[] = "Colour for";
// Each segment's sublabel, under its mode's name: on Edge's own mode, the one
// the page wears once the editor closes, and on the selected mode while it is
// not Edge's, the one the page shows until then.
inline constexpr char kEdgeMarkerLabel[] = "Edge";
inline constexpr char kPreviewMarkerLabel[] = "Preview";
// The same in full while the selector previews the mode Edge is not in, as the
// group's title and in a visually hidden live region (taking no room in the
// column); {preview} and {edge} are the two modes' labels.
inline constexpr char kPreviewStatus[] =
    "Previewing {preview} \xC2\xB7 the page follows Edge ({edge}) when you "
    "close";
// A segment's apply waits this long after the page published the previewed
// mode, so the DLL reads the new mode from the page's address when the
// editor's apply reaches it.
inline constexpr int kPreviewDelayMs = 150;
// After the section is shown, or Edge's mode flips under an untouched
// selector, the wheel is kept on the saved colour this long (Edge sets the
// remix's stored colour on it a moment after it is shown).
inline constexpr int kWheelSyncWindowMs = 1500;

// Neutral: the page's own tokens. The skin's roles are added on top of it.
// The editor's right column is exactly as tall as the wheel (172 px) and was
// full with the 40 px Dark button the selector replaces; the zoom row below
// gives up 8 px of its own (36 to 28), so the segments (30), the reset row
// (16) and the gap between them (2) take 48 and the fonts stay whole. A
// segment holds its mode's name and, under it, one line both sublabels share,
// kept whether or not one shows, so selecting or previewing a mode moves
// nothing; the full preview sentence takes no room at all.
inline constexpr char kSelectorCss[] =
    ".lights-button { display: none !important; }"
    ".edgeplus-mode-pick { box-sizing: border-box; display: flex;"
    " flex-direction: column; gap: 2px; }"
    ".edgeplus-mode-row { display: flex; gap: 4px; height: 30px; }"
    ".edgeplus-mode-pick button[data-mode] { display: grid;"
    " align-content: center; justify-items: center; }"
    ".edgeplus-mode-edge, .edgeplus-mode-preview { grid-row: 2;"
    " grid-column: 1; visibility: hidden;"
    " font-family: ui-monospace, Consolas, monospace; font-size: 10px;"
    " font-weight: 400; opacity: 0.75; }"
    ".edgeplus-mode-pick button[data-edge=\"true\"] .edgeplus-mode-edge,"
    " .edgeplus-mode-pick button[aria-checked=\"true\"][data-edge=\"false\"]"
    " .edgeplus-mode-preview { visibility: visible; }"
    ".edgeplus-mode-status { position: absolute; width: 1px; height: 1px;"
    " margin: -1px; padding: 0; border: 0; overflow: hidden;"
    " clip-path: inset(50%); white-space: nowrap; }"
    ".edgeplus-mode-pick button { flex: 1 1 0; min-width: 0; padding: 0;"
    " font: inherit; font-size: 12px; line-height: 1.1; color: inherit;"
    " cursor: pointer; background: transparent;"
    " border: 1px solid var(--smtc-stroke-ctrl-on-neutral-rest, currentColor); "
    "}"
    ".edgeplus-mode-pick button[aria-checked=\"true\"] {"
    " background: var(--smtc-background-ctrl-subtle-hover, transparent);"
    " border-color: currentColor; font-weight: 600; }"
    ".edgeplus-mode-reset { display: flex; gap: 4px; height: 16px; }"
    ".edgeplus-mode-pick .edgeplus-mode-reset button { font-size: 10px; }"
    ".edgeplus-mode-pick button:disabled { cursor: default; opacity: 0.4; }";
inline constexpr char kSkinnedSelectorCss[] =
    ".edgeplus-mode-pick button { border: 2px solid var(--ep-off);"
    " color: var(--ep-fg); border-radius: 0;"
    " font-family: \"Share Tech Mono\", Consolas, monospace;"
    " letter-spacing: 0.06em; text-transform: uppercase;"
    " clip-path: polygon(7px 0, 100% 0, 100% calc(100% - 7px),"
    " calc(100% - 7px) 100%, 0 100%, 0 7px); }"
    ".edgeplus-mode-pick button:not(:disabled):hover {"
    " border-color: var(--ep-on); }"
    ".edgeplus-mode-pick button:focus-visible { outline: 2px solid "
    "var(--ep-on);"
    " outline-offset: -5px; }"
    ".edgeplus-mode-pick button[aria-checked=\"true\"] {"
    " border-color: var(--ep-accent);"
    " background: color-mix(in srgb, var(--ep-accent) 22%, transparent); }"
    ".edgeplus-mode-edge, .edgeplus-mode-preview {"
    " font-family: \"Share Tech Mono\", Consolas, monospace;"
    " letter-spacing: 0.06em; color: var(--ep-fg); }";

[[nodiscard]] inline std::string SelectorStyleFor(bool isSkinned) {
  return std::string(kSelectorCss) + (isSkinned ? kSkinnedSelectorCss : "");
}

// The names and timings the selector script shares with the DLL and the
// page's world, as the script's opening `var`s (each statement begins with
// its `;`).
[[nodiscard]] inline std::string SelectorNames() {
  std::string script;
  script += "; var fragmentKey = " +
            web_text::JsStringLiteral(remix_colors::kFragmentKey);
  script += "; var previewKey = " +
            web_text::JsStringLiteral(remix_colors::kPreviewKey);
  script += "; var modes = {light: " +
            web_text::JsStringLiteral(remix_colors::kLightName) +
            ", dark: " + web_text::JsStringLiteral(remix_colors::kDarkName) +
            "};";
  script += " var edgeLabel = " + web_text::JsStringLiteral(kEdgeMarkerLabel);
  script +=
      "; var previewLabel = " + web_text::JsStringLiteral(kPreviewMarkerLabel);
  script += "; var previewText = " + web_text::JsStringLiteral(kPreviewStatus);
  script += "; var rowClass = " + web_text::JsStringLiteral(kSelectorRowClass);
  script +=
      "; var edgeClass = " + web_text::JsStringLiteral(kSelectorEdgeClass);
  script += "; var previewClass = " +
            web_text::JsStringLiteral(kSelectorPreviewClass);
  script +=
      "; var statusClass = " + web_text::JsStringLiteral(kSelectorStatusClass);
  script +=
      "; var resetClass = " +
      web_text::JsStringLiteral(remix_selector_reset::kSelectorResetClass);
  script += "; var resetGlyph = " +
            web_text::JsStringLiteral(remix_selector_reset::kResetGlyph);
  script += "; var resetBothLabel = " +
            web_text::JsStringLiteral(remix_selector_reset::kResetBothLabel);
  script += "; var resetOneTitle = " +
            web_text::JsStringLiteral(remix_selector_reset::kResetOneTitle);
  script += "; var resetBothTitle = " +
            web_text::JsStringLiteral(remix_selector_reset::kResetBothTitle);
  script += "; var noColourText = " +
            web_text::JsStringLiteral(
                remix_colors::FormatColour(remix_colors::kNoColour));
  script += "; var previewDelay = " + std::to_string(kPreviewDelayMs);
  script += "; var restoreDelay = " +
            std::to_string(remix_selector_leave::kRestoreDelayMs);
  script += "; var wheelEvent = " +
            web_text::JsStringLiteral(remix_wheel_bridge::kWheelEventName);
  script += "; var wheelAttribute = " +
            web_text::JsStringLiteral(remix_wheel_bridge::kWheelAttribute);
  script += "; var artifactEvent = " +
            web_text::JsStringLiteral(remix_wheel_bridge::kArtifactEventName);
  script += "; var artifactAttribute = " +
            web_text::JsStringLiteral(remix_wheel_bridge::kArtifactAttribute);
  script += "; var wheelSyncWindow = " + std::to_string(kWheelSyncWindowMs);
  return script;
}

// `pairText` is remix_colors::FormatPair of the saved pair: what the section
// opens on when it edits a stored remix. A new appearance opens on no colour.
[[nodiscard]] inline std::string SelectorScript(bool isSkinned,
                                                const std::string& pairText) {
  std::string script = "(function () { var css = ";
  script += web_text::JsStringLiteral(SelectorStyleFor(isSkinned));
  script += "; var seed = " + web_text::JsStringLiteral(pairText);
  script += "; var labels = {light: " + web_text::JsStringLiteral(kLightLabel);
  script += ", dark: " + web_text::JsStringLiteral(kDarkLabel) + "};";
  script += " var groupLabel = " + web_text::JsStringLiteral(kSelectorLabel);
  script += "; var groupClass = " + web_text::JsStringLiteral(kSelectorClass);
  script +=
      "; var styleClass = " + web_text::JsStringLiteral(kSelectorStyleClass);
  script += "; var key = " + web_text::JsStringLiteral(kSelectorStateKey);
  script += SelectorNames();
  script += R"JS(;
    function parseOne(text) {
      var found = /^(\d+),(\d+)$/.exec(text || "");
      return found ? {h: Number(found[1]), i: Number(found[2])} : null;
    }
    function parse(text) {
      var found = /^light=([^;]*);dark=([^;]*)/.exec(text) || [];
      var pair = {};
      pair[modes.light] = parseOne(found[1]);
      pair[modes.dark] = parseOne(found[2]);
      return pair;
    }
    function formatOne(colour) { return colour ? colour.h + "," + colour.i : ""; }
    function edgeMode() {
      return window.matchMedia("(prefers-color-scheme: dark)").matches ? modes.dark : modes.light;
    }
    // Each time the Appearance section is shown it starts on Edge's mode, and
    // follows Edge's flips until the selector is used. The selected mode is the
    // one the wheel edits and the page shows: it goes to the DLL with the pair.
    // The pair is set as the section opens: the saved pair when it edits a
    // stored remix, no colour when it is a new appearance (isNew).
    var state = window[key];
    if (!state) {
      state = window[key] = {pair: parse(""), isNew: false, last: null, settleUntil: 0,
          current: edgeMode(), isTouched: false, wasShown: false,
          publishedPreview: "", picker: null, group: null, isLeaving: false,
          syncUntil: 0, isWheelTouched: false, wired: new WeakSet()};
    }
    function previewNow() { return state.wasShown ? state.current : ""; }
    function publish() {
      state.publishedPreview = previewNow();
      var fragment = fragmentKey + "light=" + formatOne(state.pair[modes.light]) +
          ";dark=" + formatOne(state.pair[modes.dark]) + previewKey + state.publishedPreview;
      if (location.hash !== fragment) {
        history.replaceState(null, "", location.pathname + location.search + fragment);
      }
    }
    function readWheel(picker) {
      var found = /(\d+)\D+(\d+)/.exec(picker.getAttribute("aria-valuetext") || "");
      return found ? {h: Number(found[1]), i: Number(found[2])} : null;
    }
    // The knob sits at an angle and a distance from the picker's centre, in the
    // picker's 172-unit box; 60 units is full intensity.
    function driveWheel(picker, colour) {
      var box = picker.getBoundingClientRect();
      var scale = box.width / 172;
      var middleX = box.left + box.width / 2;
      var middleY = box.top + box.height / 2;
      var radius = colour.i / 100 * 60 * scale;
      var angle = colour.h * Math.PI / 180;
      var x = middleX + Math.cos(angle) * radius;
      var y = middleY + Math.sin(angle) * radius;
      function pointer(type, pointX, pointY) {
        return new PointerEvent(type, {bubbles: true, cancelable: true, composed: true,
            clientX: pointX, clientY: pointY, pointerId: 1, pointerType: "mouse",
            isPrimary: true, button: 0, buttons: type === "pointerup" ? 0 : 1});
      }
      var knob = picker.querySelector(".color-knob") || picker;
      knob.dispatchEvent(pointer("pointerdown", middleX, middleY));
      picker.dispatchEvent(pointer("pointermove", x, y));
      picker.dispatchEvent(pointer("pointerup", x, y));
      state.settleUntil = Date.now() + 400;
    }
    // One apply of the selected mode: the wheel is driven to that mode's colour
    // (or pressed where it is, when the mode has none yet), which makes the
    // editor apply; the DLL gives the apply the selected mode. It waits for the
    // mode published just before to reach the DLL. A new appearance with
    // nothing picked yet has no colour to press: its wheel is Edge's blank one.
    function applySelected() {
      setTimeout(function () {
        var picker = state.picker;
        if (!picker || !picker.isConnected) { return; }
        var isWheelBlank = state.isNew && !state.pair[modes.light] && !state.pair[modes.dark];
        var colour = state.pair[state.current] || (isWheelBlank ? null : readWheel(picker));
        if (colour) { driveWheel(picker, colour); }
      }, previewDelay);
    }
    // The wheel's own value, as the user picks it: kept for the selected mode
    // and published at once, so the DLL has it when the editor's apply comes.
    // Only the user's own press or key on the wheel since the section opened
    // makes a pick; Edge setting the wheel (a stored remix's colour, restored
    // whenever its component's parameters land) and the script's own moves
    // never do, and a mode switched to keeps no colour it was not given.
    function takeWheel(picker) {
      var wheel = readWheel(picker);
      if (!wheel) { return; }
      var isSame = state.last && state.last.h === wheel.h && state.last.i === wheel.i;
      if (isSame) { return; }
      state.last = wheel;
      var isPick = state.isWheelTouched && Date.now() >= state.settleUntil;
      if (isPick) {
        state.pair[state.current] = wheel;
        publish();
      }
    }
    function watchWheel(picker) {
      if (state.wired.has(picker)) { return; }
      state.wired.add(picker);
      new MutationObserver(function () {
        var isShown = window[key] === state && picker.getClientRects().length > 0;
        if (isShown) { takeWheel(picker); }
      }).observe(picker, {attributes: true, attributeFilter: ["aria-valuetext"]});
      function touch(event) {
        if (event.isTrusted) { state.isWheelTouched = true; }
      }
      picker.addEventListener("pointerdown", touch, true);
      picker.addEventListener("keydown", touch, true);
    }
    // What the section edits, from the page's world (the DLL's bridge,
    // WheelBridgeScript): the stored remix's id, "" for a new appearance, or
    // null while the bridge has not answered.
    function askArtifact(picker) {
      var host = picker.getRootNode().host;
      if (!host) { return null; }
      host.removeAttribute(artifactAttribute);
      host.dispatchEvent(new Event(artifactEvent, {bubbles: true, composed: true}));
      return host.getAttribute(artifactAttribute);
    }
    // The wheel shows the selected mode's saved colour, with no apply: the
    // wheel's own component sets its knob in the page's world (the DLL's
    // bridge, WheelBridgeScript), asked through an attribute and an event on
    // the shared DOM. On opening it is asked again for a moment, as Edge
    // restores the remix's stored colour on the wheel a little later; never
    // once the user has touched the wheel.
    function showSavedColour(picker) {
      var colour = state.pair[state.current];
      var host = picker.getRootNode().host;
      if (!colour || !host || state.isWheelTouched) { return; }
      var wheel = readWheel(picker);
      var isShown = wheel && Math.abs(wheel.h - colour.h) <= 1 && Math.abs(wheel.i - colour.i) <= 1;
      if (isShown) { return; }
      state.settleUntil = Date.now() + 400;
      host.setAttribute(wheelAttribute, colour.h + "," + colour.i);
      host.dispatchEvent(new Event(wheelEvent, {bubbles: true, composed: true}));
    }
    function labelOf(mode) { return mode === modes.dark ? labels.dark : labels.light; }
    function mark(group) {
      var edge = edgeMode();
      group.querySelectorAll("button[data-mode]").forEach(function (button) {
        var mode = button.dataset.mode;
        button.setAttribute("aria-checked", mode === state.current ? "true" : "false");
        button.dataset.edge = mode === edge ? "true" : "false";
      });
      // The segments' sublabels say it in CSS; the sentence is for a hover
      // and for a screen reader.
      var isPreviewingOther = state.current !== edge;
      var sentence = isPreviewingOther ? previewText
          .replace("{preview}", labelOf(state.current)).replace("{edge}", labelOf(edge)) : "";
      var status = group.querySelector("." + statusClass);
      if (status) { setText(status, sentence); }
      if (group.title !== sentence) { group.title = sentence; }
      markResets(group);
    }
    function sublabel(className, text) {
      var span = document.createElement("span");
      span.className = className;
      span.textContent = text;
      return span;
    }
    function build(button) {
      var group = document.createElement("div");
      group.className = groupClass;
      var row = document.createElement("div");
      row.className = rowClass;
      row.setAttribute("role", "radiogroup");
      row.setAttribute("aria-label", groupLabel);
      [modes.light, modes.dark].forEach(function (mode) {
        var choice = document.createElement("button");
        choice.type = "button";
        choice.setAttribute("role", "radio");
        choice.dataset.mode = mode;
        choice.append(labelOf(mode), sublabel(edgeClass, edgeLabel),
            sublabel(previewClass, previewLabel));
        choice.addEventListener("click", function () {
          state.isTouched = true;
          state.current = mode;
          publish();
          mark(group);
          applySelected();
        });
        row.append(choice);
      });
      var status = document.createElement("div");
      status.className = statusClass;
      status.setAttribute("role", "status");
      group.append(row, buildResets(), status);
      button.parentNode.insertBefore(group, button);
      return group;
    }
)JS";
  script += remix_selector_leave::SelectorLeaveScript();
  script += remix_selector_reset::SelectorResetScript();
  script += R"JS(
    function visit(root) {
      var button = root.querySelector(".lights-button");
      var picker = root.querySelector(".color-picker");
      if (button && button.parentNode && picker) {
        var style = root.querySelector("style." + styleClass);
        if (!style) {
          style = document.createElement("style");
          style.className = styleClass;
          root.prepend(style);
        }
        if (style.textContent !== css) { style.textContent = css; }
        var group = button.parentNode.querySelector("." + groupClass) || build(button);
        state.picker = picker;
        state.group = group;
        watchWheel(picker);
        // Leaving the section takes it out of the page (Edge keeps the component
        // and shows it again), so each time it is shown again counts as opening
        // it: Edge's mode selected, nothing applied. Editing a stored remix opens
        // on the saved pair, the wheel kept on the selected mode's colour; a new
        // appearance opens on no colour, the wheel as Edge set it. Until the
        // page's world says which, the section is not counted as open.
        var isShown = picker.getClientRects().length > 0;
        var isOpening = isShown && !state.wasShown;
        if (isOpening) {
          var artifact = askArtifact(picker);
          var isAnswered = artifact !== null;
          if (isAnswered) {
            state.isNew = artifact === "";
            state.pair = parse(state.isNew ? "" : seed);
            state.isTouched = false;
            state.isWheelTouched = false;
            state.current = edgeMode();
            state.last = null;
            state.syncUntil = Date.now() + wheelSyncWindow;
          } else {
            isShown = false;
          }
        }
        state.wasShown = isShown;
        isAnyShown = isAnyShown || isShown;
        var isFollowing = !state.isTouched && state.current !== edgeMode();
        if (isFollowing) {
          state.current = edgeMode();
          state.syncUntil = Date.now() + wheelSyncWindow;
        }
        mark(group);
        var isSyncing = isShown && !state.isNew && !state.isTouched && Date.now() < state.syncUntil;
        if (isSyncing) { showSavedColour(picker); }
        if (isShown) { takeWheel(picker); }
      }
      root.querySelectorAll("*").forEach(function (element) {
        if (element.shadowRoot) { visit(element.shadowRoot); }
      });
    }
    var isAnyShown = false;
    visit(document);
    // The section is taken out of the page when the editor is left.
    if (!isAnyShown) { state.wasShown = false; }
    if (previewNow() !== state.publishedPreview) { publish(); }
  })();)JS";
  return script;
}

[[nodiscard]] inline std::string SelectorRemoveScript() {
  std::string script = "(function () { var selector = ";
  script += web_text::JsStringLiteral(std::string(".") + kSelectorClass +
                                      ", style." + kSelectorStyleClass);
  script += "; var key = " + web_text::JsStringLiteral(kSelectorStateKey);
  script += R"JS(;
    function strip(root) {
      root.querySelectorAll(selector).forEach(function (node) { node.remove(); });
      root.querySelectorAll("*").forEach(function (element) {
        if (element.shadowRoot) { strip(element.shadowRoot); }
      });
    }
    strip(document);
    delete window[key];
  })();)JS";
  return script;
}

}  // namespace edgeplus::remix_selector
