// remix_skin.h — the Cyberpunk skin for Create a remix's pages (edge://blueprint,
// the bubble's Appearance editor, and edge://remix), and the scripts that put it
// there and take it away (docs/re/remix-theme.md). The pages are WebUI built of
// Fluent `mai-button`s and plates whose colours are `--smtc-*` custom properties
// and whose shapes are rounded; the skin repoints the properties it can and
// restyles the shapes it cannot, as the Cyberpunk UX kit's controls.css draws
// them: chamfered sharp plates, 2px outlines, mono labels, a red wash for what
// is picked. Every colour is a role of cyberpunk_palette.h. Pure: strings only.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include "find/web_text.h"
#include "theme/cyberpunk_palette.h"
#include "theme/remix_colors.h"
#include "theme/theme_tone.h"

namespace edgeplus::remix_skin {

using cyberpunk_palette::Role;
using theme_tone::Argb;
using theme_tone::Tone;

// The one gate: the pages are touched only while the remix switch is on AND the
// Cyberpunk skin is active (tab_shape::g_isCyberpunkSkinOn, the flag every skin
// hook reads). With the skin off they keep Edge's stock look; the light or dark
// follow (remix_mode.h) does not depend on it.
[[nodiscard]] constexpr bool ShouldSkin(bool isRemixSwitchOn,
                                        bool isSkinActive) noexcept {
  return isRemixSwitchOn && isSkinActive;
}

// The page script's marker on every <style> it adds, one per shadow root.
inline constexpr char kStyleClass[] = "edgeplus-remix-skin";
// The page script's state between runs (its observers and the current text).
inline constexpr char kStateKey[] = "__edgeplusRemixSkin";

inline constexpr uint32_t kChannelMask = 0xff;
inline constexpr int kAlphaShift = 24;
inline constexpr int kRedShift = 16;
inline constexpr int kGreenShift = 8;
inline constexpr uint32_t kOpaque = 0xff;
inline constexpr double kAlphaUnit = 255.0;
inline constexpr int kAlphaDigits = 3;

[[nodiscard]] inline std::string Decimal(double value) {
  std::string text = std::to_string(value);
  // "0.219608" -> "0.220": enough for a wash.
  const size_t point = text.find('.');
  if (point != std::string::npos) {
    text.resize(point + 1 + kAlphaDigits);
  }
  return text;
}

// "#rrggbb" for an opaque colour, "rgb(r g b / a)" for one with alpha.
[[nodiscard]] inline std::string CssColour(Argb colour) {
  const uint32_t alpha = (colour >> kAlphaShift) & kChannelMask;
  const uint32_t red = (colour >> kRedShift) & kChannelMask;
  const uint32_t green = (colour >> kGreenShift) & kChannelMask;
  const uint32_t blue = colour & kChannelMask;
  if (alpha == kOpaque) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string hex = "#";
    for (const uint32_t channel : {red, green, blue}) {
      hex += kDigits[channel >> 4];
      hex += kDigits[channel & 0xf];
    }
    return hex;
  }
  return "rgb(" + std::to_string(red) + " " + std::to_string(green) + " " +
         std::to_string(blue) + " / " +
         Decimal(static_cast<double>(alpha) / kAlphaUnit) + ")";
}

// One palette role, as the custom property the structure below reads.
struct RoleVariable {
  std::string_view name;
  Role role;
};

inline constexpr std::array<RoleVariable, 12> kRoleVariables = {{
    {"--ep-panel", Role::kPanel},
    {"--ep-fg", Role::kForeground},
    {"--ep-fg-muted", Role::kForegroundMuted},
    {"--ep-on", Role::kOn},
    {"--ep-off", Role::kOff},
    {"--ep-frame", Role::kFrame},
    {"--ep-accent", Role::kAccent},
    {"--ep-label", Role::kLabel},
    {"--ep-hover", Role::kHover},
    {"--ep-alert", Role::kAlert},
    {"--ep-on-fill", Role::kOnFill},
    {"--ep-ink", Role::kInk},
}};

// What the pages already read, repointed at those roles, so a part the
// structure below does not name (the side panel's lists) still comes out in the
// skin's colours.
struct PageToken {
  std::string_view name;
  std::string_view roleVariable;
};

inline constexpr std::array<PageToken, 41> kPageTokens = {{
    {"--smtc-background-flyout-solid", "--ep-panel"},
    {"--smtc-background-ctrl-neutral-rest", "--ep-panel"},
    {"--smtc-background-ctrl-neutral-hover", "--ep-hover"},
    {"--smtc-background-ctrl-neutral-pressed", "--ep-hover"},
    {"--smtc-background-ctrl-subtle-hover", "--ep-hover"},
    {"--smtc-background-ctrl-subtle-pressed", "--ep-hover"},
    {"--smtc-foreground-ctrl-neutral-primary-rest", "--ep-fg"},
    {"--smtc-foreground-ctrl-neutral-primary-hover", "--ep-fg"},
    {"--smtc-foreground-ctrl-neutral-primary-pressed", "--ep-fg"},
    {"--smtc-foreground-content-neutral-primary", "--ep-fg"},
    {"--smtc-foreground-ctrl-on-subtle-rest", "--ep-fg-muted"},
    {"--smtc-stroke-ctrl-on-neutral-rest", "--ep-off"},
    {"--smtc-stroke-ctrl-divider-on-brand", "--ep-frame"},
    {"--smtc-ctrl-focus-outer-stroke", "--ep-on"},
    // Fluent's brand blue, the pages' pale-blue fills and links: a mark in the
    // skin's red (a selected tab, a checked box), links in the "on" colour.
    {"--smtc-background-ctrl-brand-rest", "--ep-accent"},
    {"--smtc-background-ctrl-brand-hover", "--ep-accent"},
    {"--smtc-background-ctrl-brand-pressed", "--ep-accent"},
    {"--smtc-background-ctrl-active-brand-rest", "--ep-accent"},
    {"--smtc-background-ctrl-active-brand-hover", "--ep-accent"},
    {"--smtc-background-ctrl-active-brand-pressed", "--ep-accent"},
    {"--smtc-foreground-ctrl-brand-rest", "--ep-on"},
    {"--smtc-foreground-ctrl-brand-hover", "--ep-on"},
    {"--smtc-foreground-ctrl-brand-pressed", "--ep-on"},
    {"--smtc-foreground-ctrl-active-brand-rest", "--ep-on"},
    {"--smtc-foreground-ctrl-active-brand-hover", "--ep-on"},
    {"--smtc-foreground-ctrl-active-brand-pressed", "--ep-on"},
    {"--smtc-foreground-ctrl-on-brand-rest", "--ep-fg"},
    {"--smtc-foreground-ctrl-on-brand-hover", "--ep-fg"},
    {"--smtc-foreground-ctrl-on-brand-pressed", "--ep-fg"},
    {"--smtc-foreground-ctrl-on-active-brand-rest", "--ep-fg"},
    {"--smtc-foreground-ctrl-on-active-brand-hover", "--ep-fg"},
    {"--smtc-foreground-ctrl-on-active-brand-pressed", "--ep-fg"},
    {"--smtc-stroke-divider-default", "--ep-off"},
    {"--smtc-stroke-divider-subtle", "--ep-off"},
    // The editor's own dark-mode rules read these, with fixed greys as
    // fallbacks.
    {"--blueprint-dark-flyout-background", "--ep-panel"},
    {"--blueprint-dark-flyout-border", "--ep-panel"},
    {"--blueprint-dark-divider", "--ep-off"},
    {"--blueprint-dark-neutral-border", "--ep-off"},
    {"--blueprint-dark-neutral-hover", "--ep-hover"},
    {"--blueprint-dark-neutral-selected", "--ep-hover"},
    {"--blueprint-dark-foreground-primary", "--ep-fg"},
}};

// The shapes. `!important` because the components' own `:host` and
// `:host([dark-mode])` rules out-rank a plain selector from outside.
//
// - A control or plate: no radius, a 2px idle line in the "off" teal, mono,
//   the kit's 7px chamfer; hover lights the line (never a fill).
// - Picked (a pressed toggle, the chosen font, a pressed card): the red wash and
//   a red outline; red is for marks and fills, never text.
// - Icon-only controls and the editor's title bar stay bare until hovered.
// - Cyan fills nothing here: this is a floating surface (palette.h).
inline constexpr char kStructureCss[] = R"CSS(
mai-button {
  border-radius: 0 !important;
  corner-shape: round !important;
  border: 2px solid var(--ep-off) !important;
  background: transparent !important;
  color: var(--ep-fg) !important;
  box-shadow: none !important;
  font-family: "Share Tech Mono", Consolas, monospace !important;
  letter-spacing: 0.06em;
  clip-path: polygon(7px 0, 100% 0, 100% calc(100% - 7px), calc(100% - 7px) 100%, 0 100%, 0 7px);
}
mai-button::after { display: none !important; }
mai-button:hover { border-color: var(--ep-on) !important; background: transparent !important; }
mai-button:focus-visible {
  border-color: var(--ep-on) !important;
  outline: 2px solid var(--ep-on) !important;
  outline-offset: -5px !important;
  box-shadow: none !important;
}
mai-button[icon-only], mai-button[appearance="subtle"] { border-color: transparent !important; }
mai-button[icon-only]:hover, mai-button[appearance="subtle"]:hover { border-color: var(--ep-on) !important; }
mai-button[aria-pressed="true"], mai-button[aria-checked="true"], mai-button[appearance="primary"] {
  border-color: var(--ep-accent) !important;
  background: color-mix(in srgb, var(--ep-accent) 22%, transparent) !important;
  color: var(--ep-fg) !important;
}
mai-button[disabled], mai-button[aria-disabled="true"] {
  border-color: var(--ep-off) !important;
  color: var(--ep-fg-muted) !important;
  opacity: 0.6;
}
.card {
  border-radius: 0 !important;
  border: 2px solid var(--ep-off) !important;
  background: transparent !important;
  box-shadow: none !important;
  clip-path: polygon(12px 0, 100% 0, 100% calc(100% - 12px), calc(100% - 12px) 100%, 0 100%, 0 12px);
}
.card:hover { border-color: var(--ep-on) !important; background: transparent !important; }
.card:focus-visible { outline-offset: -6px !important; outline-color: var(--ep-on) !important; }
.card-icon { color: var(--ep-on); }
.card-title, .title, .card-subtitle, .description {
  color: var(--ep-fg) !important;
}
.title, .card-title {
  font-family: "Chakra Petch", "Segoe UI", sans-serif !important;
  text-transform: uppercase;
  letter-spacing: 0.07em;
}
.card-subtitle, .description {
  font-family: "Share Tech Mono", Consolas, monospace !important;
  color: var(--ep-fg-muted) !important;
}
widget-adjustment, blueprint-main {
  border-radius: 0 !important;
  border: 0 !important;
  box-shadow: none !important;
  backdrop-filter: none !important;
  background: var(--ep-panel) !important;
}
.divider {
  height: 2px !important;
  background: linear-gradient(var(--ep-accent), var(--ep-accent)) left / 44px 100% no-repeat, var(--ep-off) !important;
}
.zoom-controls, .type-selector {
  border: 2px solid var(--ep-off) !important;
  border-radius: 0 !important;
  background: transparent !important;
  box-shadow: none !important;
  clip-path: polygon(12px 0, 100% 0, 100% calc(100% - 12px), calc(100% - 12px) 100%, 0 100%, 0 12px);
}
.zoom-controls mai-button, .type-selector mai-button { clip-path: none; }
.zoom-indicator, .type-label { color: var(--ep-fg) !important; }
.type-button[aria-checked="true"], .type-button[aria-checked="true"]::part(control) {
  background: color-mix(in srgb, var(--ep-accent) 22%, transparent) !important;
  box-shadow: none !important;
}
mai-switch {
  box-sizing: border-box !important;
  width: 46px !important;
  height: 22px !important;
  padding: 0 3px !important;
  border: 2px solid var(--ep-off) !important;
  border-radius: 0 !important;
  background: transparent !important;
  box-shadow: none !important;
  clip-path: polygon(6px 0, 100% 0, 100% calc(100% - 6px), calc(100% - 6px) 100%, 0 100%, 0 6px);
}
mai-switch::before { display: none !important; }
mai-switch:hover { border-color: var(--ep-on) !important; }
mai-switch:focus-visible { outline: 2px solid var(--ep-on) !important; outline-offset: -5px !important; }
mai-switch:is([state--checked], :state(checked)) {
  border-color: var(--ep-on) !important;
  background: color-mix(in srgb, var(--ep-on-fill) 22%, transparent) !important;
}
.checked-indicator {
  border-radius: 0 !important;
  box-shadow: none !important;
}
mai-switch .checked-indicator, .checked-indicator[part="checked-indicator"] {
  width: 12px !important;
  height: 12px !important;
  background: var(--ep-off) !important;
}
:host(:is([state--checked], :state(checked))) .checked-indicator[part="checked-indicator"] {
  background: var(--ep-on) !important;
}
mai-checkbox {
  border-radius: 0 !important;
  border-color: var(--ep-off) !important;
  box-shadow: none !important;
}
mai-checkbox:hover { border-color: var(--ep-on) !important; }
mai-checkbox:is([state--checked], :state(checked)) {
  border-color: var(--ep-accent) !important;
  background: color-mix(in srgb, var(--ep-accent) 22%, transparent) !important;
}
mai-link { color: var(--ep-on) !important; }
mai-link:hover { color: var(--ep-fg) !important; }
mai-tab { border-radius: 0 !important; background: transparent !important; font-family: "Share Tech Mono", Consolas, monospace !important; }
.toast { border-radius: 0 !important; border: 2px solid var(--ep-off) !important; background: var(--ep-panel) !important; box-shadow: none !important; }
.color-picker-inner { fill: var(--ep-panel) !important; stroke: var(--ep-frame) !important; stroke-width: 2px !important; }
.color-picker-inner-shadow { box-shadow: none !important; }
.color-picker:focus-visible { outline: 2px solid var(--ep-on); outline-offset: 2px; border-radius: 50%; }
)CSS";

[[nodiscard]] inline std::string DeclarationsOf(std::string_view selector,
                                                const std::string& body) {
  return std::string(selector) + " {" + body + "}\n";
}

// The complete style text for `tone`: the role variables on the root, the
// page's own tokens pointed at them, then the shapes.
[[nodiscard]] inline std::string StyleFor(Tone tone) {
  std::string variables;
  for (const RoleVariable& variable : kRoleVariables) {
    variables += std::string(variable.name) + ": " +
                 CssColour(cyberpunk_palette::ColourOf(variable.role, tone)) +
                 ";";
  }
  // The root's own rule is doubled to out-rank the page's colors.css.
  variables +=
      tone == Tone::kLight ? "color-scheme: light;" : "color-scheme: dark;";
  std::string css = DeclarationsOf(":root:root", variables);
  std::string tokens;
  for (const PageToken& token : kPageTokens) {
    tokens += std::string(token.name) + ": var(" +
              std::string(token.roleVariable) + ");";
  }
  css += DeclarationsOf(":root:root", tokens);
  css += kStructureCss;
  return css;
}

// Adds (or refreshes) one <style> in the document and in every open shadow
// root under it, and leaves a MutationObserver on each of them so a root built
// later (the editor's components render after the script first runs) is styled
// in the same microtask, before the browser paints it; C++ only has to deliver
// this once per page. Runs in our isolated world, which the page's CSP does not
// govern; the DOM is shared. Idempotent: running it again only swaps the text.
[[nodiscard]] inline std::string PaintScript(Tone tone) {
  std::string script = "(function () {\n  var css = ";
  script += web_text::JsStringLiteral(StyleFor(tone));
  script += ";\n  var cls = " + web_text::JsStringLiteral(kStyleClass) + ";";
  script += "\n  var key = " + web_text::JsStringLiteral(kStateKey) + ";";
  script += R"JS(
    var state = window[key] || (window[key] = {css: css, watched: new WeakSet(), observers: []});
    state.css = css;
    function paint(root, parent) {
      var style = null;
      for (var i = 0; i < parent.children.length; i++) {
        if (parent.children[i].className === cls) { style = parent.children[i]; }
      }
      if (!style) {
        style = document.createElement("style");
        style.className = cls;
        parent.prepend(style);
      }
      if (style.textContent !== state.css) { style.textContent = state.css; }
      watch(root);
      root.querySelectorAll("*").forEach(function (element) {
        if (element.shadowRoot) { paint(element.shadowRoot, element.shadowRoot); }
      });
    }
    function watch(root) {
      if (state.watched.has(root)) { return; }
      state.watched.add(root);
      var observer = new MutationObserver(function () {
        paint(document, document.head || document.documentElement);
      });
      observer.observe(root, {childList: true, subtree: true});
      state.observers.push(observer);
    }
    paint(document, document.head || document.documentElement);
  })();)JS";
  return script;
}

// --- the light / dark colour selector (needs no skin) ---
// With the remix switch on, an Appearance remix wears one colour for Edge's light
// mode and another for its dark mode (remix_colors.h), as the Edge+ theme has a
// pair. The editor's "Dark" button, which only toggled the remix's own darkmode
// (now Edge's, remix_darkmode.h), is hidden and a Light | Dark selector takes
// its place: it says which mode the wheel is editing, and switching it moves the
// wheel to that mode's colour (the saved one when editing a stored remix, none
// yet in a new appearance until the user picks it). A row under it resets the
// selected mode's colour, or both, to no colour (the page's own look). Font and
// zoom stay shared. The page's own
// wheel is read from its slider's text and driven with pointer events (our
// isolated world cannot see the page's script objects, only the shared DOM). The
// pair goes back to the DLL in the page's address fragment.
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
// The reset row under the segments: one button for the selected mode, one for
// both. A reset gives the mode no colour (remix_colors::kNoColour); the
// remix's font and size stay. {mode} is the selected mode's label.
inline constexpr char kSelectorResetClass[] = "edgeplus-mode-reset";
inline constexpr char kResetGlyph[] = "\xE2\x86\xBA";  // anticlockwise arrow
inline constexpr char kResetBothLabel[] = "Both";
inline constexpr char kResetOneTitle[] = "Remove the {mode} colour";
inline constexpr char kResetBothTitle[] = "Remove both colours";
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
// Leaving while previewing the other mode: Edge's mode is applied first, and
// the Back or Close goes through this long after.
inline constexpr int kRestoreDelayMs = 500;
// How the editor script asks the page's world to set the wheel without an
// apply (WheelBridgeScript): the colour in this attribute of the wheel's
// component, then this event on it.
inline constexpr char kWheelEventName[] = "edgeplus-wheel";
inline constexpr char kWheelAttribute[] = "data-edgeplus-wheel";
// How the editor script asks the page's world what the opened section edits:
// this event on the wheel's component, answered (in the same dispatch) in this
// attribute of it: the component's artifactUid, the stored remix that More
// options > Edit opened, or empty for a new appearance (Create > Appearance).
inline constexpr char kArtifactEventName[] = "edgeplus-wheel-artifact";
inline constexpr char kArtifactAttribute[] = "data-edgeplus-artifact";
// Where the bridge keeps itself in the page's own world, as {remove}.
inline constexpr char kWheelBridgeKey[] = "__edgeplusWheelBridge";
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

// The part of the selector script that restores Edge's mode before the user
// leaves the section or the editor (it runs inside SelectorScript's function).
[[nodiscard]] inline std::string SelectorLeaveScript() {
  return R"JS(
    // Leaving the section (Back) or the editor (Close, Esc) while the other mode
    // is previewed: Edge's mode is applied first, and the leave goes through a
    // moment later, re-sent by script. Best effort: a close Edge handles outside
    // the page (a click outside the bubble) is not seen, and the next page
    // load applies Edge's mode anyway.
    function restoreBeforeLeaving() {
      var edge = edgeMode();
      var isPreviewingOther = state.wasShown && state.current !== edge;
      if (!isPreviewingOther) { return false; }
      state.current = edge;
      state.isTouched = false;
      publish();
      if (state.group) { mark(state.group); }
      applySelected();
      return true;
    }
    function leavingControl(event) {
      return event.composedPath().find(function (node) {
        var label = node.getAttribute ? node.getAttribute("aria-label") : "";
        return label === "Back" || label === "Close";
      });
    }
    var leaveKey = key + "Leave";
    if (!window[leaveKey]) {
      window[leaveKey] = true;
      document.addEventListener("click", function (event) {
        var live = window[key];
        if (!live || live !== state || state.isLeaving) { return; }
        var control = leavingControl(event);
        if (!control || !restoreBeforeLeaving()) { return; }
        event.preventDefault();
        event.stopImmediatePropagation();
        state.isLeaving = true;
        setTimeout(function () { control.click(); state.isLeaving = false; }, restoreDelay);
      }, true);
      document.addEventListener("keydown", function (event) {
        var live = window[key];
        if (!live || live !== state || state.isLeaving || event.key !== "Escape") { return; }
        if (!restoreBeforeLeaving()) { return; }
        event.preventDefault();
        event.stopImmediatePropagation();
        state.isLeaving = true;
        var target = event.composedPath()[0];
        setTimeout(function () {
          target.dispatchEvent(new KeyboardEvent("keydown", {key: "Escape", code: "Escape",
              keyCode: 27, bubbles: true, composed: true, cancelable: true}));
          state.isLeaving = false;
        }, restoreDelay);
      }, true);
    }
)JS";
}

// The part of the selector script that builds and runs the reset row (it runs
// inside SelectorScript's function; build() places the row, mark() keeps it).
[[nodiscard]] inline std::string SelectorResetScript() {
  return R"JS(
    // The reset buttons: the selected mode's, and both modes'.
    var resetKind = {one: "one", both: "both"};
    // A mode holds a colour to reset: set, and off the wheel's centre.
    function hasTint(mode) {
      var colour = state.pair[mode];
      return !!colour && colour.i > 0;
    }
    function setText(element, text) {
      if (element.textContent !== text) { element.textContent = text; }
    }
    function setTitle(button, title) {
      if (button.title !== title) { button.title = title; }
      if (button.getAttribute("aria-label") !== title) { button.setAttribute("aria-label", title); }
    }
    function setDisabled(button, isDisabled) {
      if (button.disabled !== isDisabled) { button.disabled = isDisabled; }
    }
    // Each reset is offered only when there is a colour to take off.
    function markResets(group) {
      var one = group.querySelector('button[data-reset="' + resetKind.one + '"]');
      var both = group.querySelector('button[data-reset="' + resetKind.both + '"]');
      if (!one || !both) { return; }
      var label = labelOf(state.current);
      setText(one, resetGlyph + " " + label);
      setTitle(one, resetOneTitle.replace("{mode}", label));
      setDisabled(one, !hasTint(state.current));
      setText(both, resetGlyph + " " + resetBothLabel);
      setTitle(both, resetBothTitle);
      setDisabled(both, !hasTint(modes.light) && !hasTint(modes.dark));
    }
    // A reset gives the mode (or both) no colour, the page's own look with the
    // remix's font and size kept, and publishes the pair for the DLL to save.
    // When the selected mode was reset it is applied once: the knob is pressed
    // at the wheel's centre, which makes the editor apply with no colour.
    function reset(resetModes) {
      resetModes.forEach(function (mode) { state.pair[mode] = parseOne(noColourText); });
      publish();
      if (state.group) { mark(state.group); }
      var isSelectedReset = resetModes.indexOf(state.current) !== -1;
      if (isSelectedReset) { applySelected(); }
    }
    function buildResets() {
      var resets = document.createElement("div");
      resets.className = resetClass;
      var one = document.createElement("button");
      one.type = "button";
      one.dataset.reset = resetKind.one;
      one.addEventListener("click", function () { reset([state.current]); });
      var both = document.createElement("button");
      both.type = "button";
      both.dataset.reset = resetKind.both;
      both.addEventListener("click", function () { reset([modes.light, modes.dark]); });
      resets.append(one, both);
      return resets;
    }
)JS";
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
      "; var resetClass = " + web_text::JsStringLiteral(kSelectorResetClass);
  script += "; var resetGlyph = " + web_text::JsStringLiteral(kResetGlyph);
  script +=
      "; var resetBothLabel = " + web_text::JsStringLiteral(kResetBothLabel);
  script +=
      "; var resetOneTitle = " + web_text::JsStringLiteral(kResetOneTitle);
  script +=
      "; var resetBothTitle = " + web_text::JsStringLiteral(kResetBothTitle);
  script += "; var noColourText = " +
            web_text::JsStringLiteral(
                remix_colors::FormatColour(remix_colors::kNoColour));
  script += "; var previewDelay = " + std::to_string(kPreviewDelayMs);
  script += "; var restoreDelay = " + std::to_string(kRestoreDelayMs);
  script += "; var wheelEvent = " + web_text::JsStringLiteral(kWheelEventName);
  script +=
      "; var wheelAttribute = " + web_text::JsStringLiteral(kWheelAttribute);
  script +=
      "; var artifactEvent = " + web_text::JsStringLiteral(kArtifactEventName);
  script += "; var artifactAttribute = " +
            web_text::JsStringLiteral(kArtifactAttribute);
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
  script += SelectorLeaveScript();
  script += SelectorResetScript();
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

// Run in the editor page's own world (the wheel's component, widget-adjustment,
// is visible only there): sets the wheel's knob to the colour the editor script
// asked for, through the component's own setKnobFromHueIntensity, which moves
// the knob and its colour without the editor applying anything (it applies only
// on a pointer release or a key), and says what the opened section edits: the
// component's artifactUid, which Edge sets to the stored remix on More options >
// Edit and to null on Create > Appearance (and sends with every apply, to update
// that remix or make a new one). Installs itself once per DLL load: the page
// outlives the DLL (a bubble reopened keeps its page), so unloading takes the
// bridge out (WheelBridgeRemoveScript) and the next DLL's goes in. A bridge
// from a build before that left a bare `true` it cannot remove; the new one goes
// in beside it (the old one only sets the knob, and takes the attribute first).
[[nodiscard]] inline std::string WheelBridgeScript() {
  std::string script = "(function () { var wheelEvent = ";
  script += web_text::JsStringLiteral(kWheelEventName);
  script +=
      "; var wheelAttribute = " + web_text::JsStringLiteral(kWheelAttribute);
  script +=
      "; var artifactEvent = " + web_text::JsStringLiteral(kArtifactEventName);
  script += "; var artifactAttribute = " +
            web_text::JsStringLiteral(kArtifactAttribute);
  script += "; var key = " + web_text::JsStringLiteral(kWheelBridgeKey);
  script += R"JS(;
    var installed = window[key];
    var isInstalled = typeof installed === "object" && installed !== null;
    if (isInstalled) { return; }
    function onWheel(event) {
      var host = event.composedPath()[0];
      var text = host && host.getAttribute ? host.getAttribute(wheelAttribute) || "" : "";
      var found = /^(\d+),(\d+)$/.exec(text);
      var canSet = found && typeof host.setKnobFromHueIntensity === "function";
      if (!canSet) { return; }
      host.removeAttribute(wheelAttribute);
      host.setKnobFromHueIntensity(Number(found[1]), Number(found[2]) / 100);
    }
    function onArtifact(event) {
      var host = event.composedPath()[0];
      var canAnswer = host && host.setAttribute && "artifactUid" in host;
      if (!canAnswer) { return; }
      host.setAttribute(artifactAttribute, host.artifactUid || "");
    }
    document.addEventListener(wheelEvent, onWheel, true);
    document.addEventListener(artifactEvent, onArtifact, true);
    window[key] = {remove: function () {
      document.removeEventListener(wheelEvent, onWheel, true);
      document.removeEventListener(artifactEvent, onArtifact, true);
    }};
  })();)JS";
  return script;
}

[[nodiscard]] inline std::string WheelBridgeRemoveScript() {
  std::string script = "(function () { var key = ";
  script += web_text::JsStringLiteral(kWheelBridgeKey);
  script += R"JS(;
    var installed = window[key];
    var canRemove = installed && typeof installed.remove === "function";
    if (canRemove) {
      installed.remove();
      delete window[key];
    }
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

[[nodiscard]] inline std::string RemoveScript() {
  std::string script = "(function () {\n  var cls = ";
  script += web_text::JsStringLiteral(kStyleClass);
  script += ";\n  var key = " + web_text::JsStringLiteral(kStateKey);
  script += R"JS(;
    function strip(root) {
      root.querySelectorAll("style." + cls).forEach(function (style) { style.remove(); });
      root.querySelectorAll("*").forEach(function (element) {
        if (element.shadowRoot) { strip(element.shadowRoot); }
      });
    }
    strip(document);
    var state = window[key];
    if (state) {
      state.observers.forEach(function (observer) { observer.disconnect(); });
      delete window[key];
    }
  })();)JS";
  return script;
}

}  // namespace edgeplus::remix_skin
