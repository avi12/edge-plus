// reading_scripts.h — the scripts reading_mode.h runs in each reading view: a
// "Light mode | Dark mode" switch above the Themes grid of reading mode's
// Appearance settings, and its removal. They run in our isolated world, which
// the page's CSP does not govern; the DOM (open shadow roots included) is
// shared, so the switch sits in Edge's own flyout, styled with its own Fluent
// tokens (docs/re/reading-mode-theme.md). Pure: strings only.
#pragma once
#include <string>
#include "reading_themes.h"
#include "web_text.h"

namespace edgeplus::reading_scripts {

inline constexpr char kGroupLabel[] = "Themes for";
inline constexpr char kLightLabel[] = "Light mode";
inline constexpr char kDarkLabel[] = "Dark mode";

// The page script's state between runs.
inline constexpr char kStateKey[] = "__edgeplusReadingMode";

// Null until reading mode has built the flyout.
inline constexpr char kPaneJs[] = R"JS(
    function pane() {
      var bar = document.querySelector("reader-mode-bar");
      var host = bar && bar.shadowRoot && bar.shadowRoot.querySelector("appearance-pane");
      return host && host.shadowRoot;
    })JS";

// Answers `ok`, or `ok US light US dark` when a theme was picked since the last
// run. `isDark` is Windows' mode: the side the switch opens on, and the side
// whose pick the page itself applies.
[[nodiscard]] inline std::string DecorateScript(
    bool isDark, const reading_themes::Choices& choices) {
  std::string script = "(function () {\n  var config = {mode: ";
  script += web_text::JsStringLiteral(isDark ? "dark" : "light");
  script += ", light: " + std::to_string(static_cast<int>(choices.light));
  script += ", dark: " + std::to_string(static_cast<int>(choices.dark));
  script += ", group: " + web_text::JsStringLiteral(kGroupLabel);
  script += ", lightLabel: " + web_text::JsStringLiteral(kLightLabel);
  script += ", darkLabel: " + web_text::JsStringLiteral(kDarkLabel);
  script += "};\n  var key = " + web_text::JsStringLiteral(kStateKey) + ";";
  script += kPaneJs;
  script += R"JS(
    var state = window[key] ||
        (window[key] = {side: null, pending: false, wired: new WeakSet()});
    if (!state.pending) { state.light = config.light; state.dark = config.dark; }
    state.mode = config.mode;

    function valueOf(event) {
      var path = event.composedPath();
      for (var i = 0; i < path.length; i++) {
        if (path[i].tagName === "FLUENT-FIELD") {
          var radio = path[i].querySelector("fluent-radio");
          return radio ? Number(radio.getAttribute("value")) : NaN;
        }
      }
      return NaN;
    }

    // The checkmark: while the switch is in the flyout the grid shows our own
    // mark (a clone of the page's), on the shown side's choice, so it never
    // depends on how the page redraws its own.
    function render(root) {
      var group = root.querySelector("fluent-radio-group");
      if (!group) {
        return;
      }
      group.setAttribute("data-edgeplus-marks", "");
      root.querySelectorAll(".edgeplus-side").forEach(function (button) {
        var isOn = button.dataset.side === state.side;
        button.setAttribute("aria-checked", String(isOn));
        button.tabIndex = isOn ? 0 : -1;
      });
      root.querySelectorAll(".edgeplus-check").forEach(function (mark) { mark.remove(); });
      var source = root.querySelector(".checked-indicator:not(.edgeplus-check)");
      if (source) {
        state.markSource = source.cloneNode(true);
      }
      var radio = group.querySelector('fluent-radio[value="' + state[state.side] + '"]');
      var box = radio && radio.closest("fluent-field").querySelector(".color-button-container");
      if (state.markSource && box) {
        var mark = state.markSource.cloneNode(true);
        mark.classList.add("edgeplus-check");
        box.append(mark);
      }
    }

    function choose(root, value) {
      if (!isFinite(value)) {
        return;
      }
      state[state.side] = value;
      state.pending = true;
      render(root);
    }

    function addSwitch(root) {
      if (!root.querySelector(".edgeplus-style")) {
        var style = document.createElement("style");
        style.className = "edgeplus-style";
        style.textContent =
          ".edgeplus-mode-switch { display: inline-flex; padding: 2px; margin: 2px 0 6px;" +
          " border: var(--smtc-stroke-width-default, 1px) solid var(--smtc-stroke-ctrl-on-outline-rest);" +
          " border-radius: var(--smtc-corner-ctrl-rest); background: var(--smtc-background-ctrl-neutral-rest); }" +
          ".edgeplus-side { font: inherit; font-size: var(--smtc-text-global-caption1-font-size);" +
          " line-height: var(--smtc-text-global-caption1-line-height); padding: 2px var(--smtc-padding-ctrl-horizontal-default);" +
          " border: 0; border-radius: var(--smtc-corner-ctrl-sm-rest); cursor: pointer;" +
          " color: var(--smtc-foreground-ctrl-neutral-secondary-rest); background: transparent; }" +
          ".edgeplus-side:hover { background: var(--smtc-background-ctrl-subtle-hover); }" +
          ".edgeplus-side[aria-checked=\"true\"] { color: var(--smtc-foreground-ctrl-on-brand-rest);" +
          " background: var(--smtc-background-ctrl-brand-rest); font-weight: 600; }" +
          ".edgeplus-side:focus-visible { outline: var(--smtc-ctrl-focus-outer-stroke-width) solid" +
          " var(--smtc-ctrl-focus-outer-stroke); outline-offset: 1px; }" +
          "[data-edgeplus-marks] .checked-indicator:not(.edgeplus-check) { display: none; }";
        root.append(style);
      }
      var title = root.querySelector(".themes-container .title-font");
      if (!title || root.querySelector(".edgeplus-mode-switch")) {
        return;
      }
      var bar = document.createElement("div");
      bar.className = "edgeplus-mode-switch";
      bar.setAttribute("role", "radiogroup");
      bar.setAttribute("aria-label", config.group);
      ["light", "dark"].forEach(function (side) {
        var button = document.createElement("button");
        button.type = "button";
        button.className = "edgeplus-side";
        button.dataset.side = side;
        button.setAttribute("role", "radio");
        button.textContent = side === "light" ? config.lightLabel : config.darkLabel;
        button.addEventListener("click", function (event) {
          event.stopPropagation();
          state.side = side;
          render(root);
        });
        bar.append(button);
      });
      bar.addEventListener("keydown", function (event) {
        var isArrow = ["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].indexOf(event.key) >= 0;
        if (!isArrow) {
          return;
        }
        event.preventDefault();
        event.stopPropagation();
        state.side = state.side === "light" ? "dark" : "light";
        render(root);
        bar.querySelector('[data-side="' + state.side + '"]').focus();
      });
      title.after(bar);
    }

    // A pick for Windows' own side is the page's (it switches and saves the
    // theme itself) and is only noted; a pick for the other side never reaches
    // the page. After RemoveScript these listeners let everything through.
    function wireGrid(root) {
      var group = root.querySelector("fluent-radio-group");
      if (!group || state.wired.has(group)) {
        return;
      }
      state.wired.add(group);
      group.addEventListener("click", function (event) {
        if (window[key] !== state) {
          return;
        }
        if (state.side === state.mode) {
          state[state.mode] = valueOf(event);
          state.pending = true;
          render(root);
          return;
        }
        event.stopPropagation();
        event.preventDefault();
        choose(root, valueOf(event));
      }, true);
      group.addEventListener("keydown", function (event) {
        if (window[key] !== state || state.side === state.mode) {
          return;
        }
        var radios = Array.prototype.slice.call(group.querySelectorAll("fluent-radio"));
        var current = event.composedPath().filter(function (node) { return node.tagName === "FLUENT-RADIO"; })[0];
        var at = radios.indexOf(current);
        var step = {ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1}[event.key];
        var isPick = event.key === " " || event.key === "Enter";
        if (!step && !isPick) {
          return;
        }
        event.stopPropagation();
        event.preventDefault();
        var next = step ? radios[(at + step + radios.length) % radios.length] : radios[at];
        if (next) {
          next.focus();
          choose(root, Number(next.getAttribute("value")));
        }
      }, true);
      group.addEventListener("change", function () {
        if (window[key] === state && state.side === state.mode) {
          state[state.mode] = Number(group.getAttribute("value"));
          state.pending = true;
          render(root);
        }
      });
    }

    // Idempotent.
    function ensure(root) {
      addSwitch(root);
      wireGrid(root);
      render(root);
    }

    var root = pane();
    var box = root && root.querySelector(".appearance-pane-container");
    var isShown = !!box && box.getClientRects().length > 0;
    if (!isShown) {
      state.side = null;
    } else {
      if (state.side === null) {
        state.side = state.mode;
      }
      ensure(root);
      // Should the page redraw the flyout (a theme change can), put the
      // switch and the mark back at once rather than at the next visit.
      if (state.observedRoot !== root) {
        if (state.observer) {
          state.observer.disconnect();
        }
        state.observedRoot = root;
        state.observer = new MutationObserver(function () {
          var isMissing = !root.querySelector(".edgeplus-mode-switch") ||
              !root.querySelector(".edgeplus-check");
          if (window[key] === state && state.side && isMissing) {
            ensure(root);
          }
        });
        state.observer.observe(root, {childList: true, subtree: true});
      }
    }
    if (!state.pending) {
      return )JS";
  script += web_text::JsOkAnswer();
  script += R"JS(;
    }
    state.pending = false;
    return )JS";
  script += web_text::JsOkAnswer({"String(state.light)", "String(state.dark)"});
  script += ";\n})()";
  return script;
}

// The grid's listeners stay until the page reloads, but with their state gone
// they pass every pick straight to the page.
[[nodiscard]] inline std::string RemoveScript() {
  std::string script = "(function () {\n  var key = ";
  script += web_text::JsStringLiteral(kStateKey) + ";";
  script += kPaneJs;
  script += R"JS(
    var root = pane();
    if (root) {
      root.querySelectorAll(".edgeplus-mode-switch, .edgeplus-style, .edgeplus-check")
          .forEach(function (node) { node.remove(); });
      var group = root.querySelector("fluent-radio-group");
      if (group) {
        group.removeAttribute("data-edgeplus-marks");
      }
    }
    if (window[key] && window[key].observer) {
      window[key].observer.disconnect();
    }
    delete window[key];
    return )JS";
  script += web_text::JsOkAnswer();
  script += ";\n})()";
  return script;
}

}  // namespace edgeplus::reading_scripts
