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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "find/web_text.h"
#include "theme/skin/cyberpunk_palette.h"
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
