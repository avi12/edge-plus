// remix_selector_reset.h — the reset row of the Appearance editor's colour
// selector (remix_selector.h), under its segments: one button for the selected
// mode, one for both. A reset gives the mode no colour
// (remix_colors::kNoColour); the remix's font and size stay. {mode} is the
// selected mode's label. Pure: strings only.
#pragma once
#include <string>
#include "find/web_text.h"
#include "theme/remix/remix_colors.h"

namespace edgeplus::remix_selector_reset {

inline constexpr char kSelectorResetClass[] = "edgeplus-mode-reset";
inline constexpr char kResetGlyph[] = "\xE2\x86\xBA";  // anticlockwise arrow
inline constexpr char kResetBothLabel[] = "Both";
inline constexpr char kResetOneTitle[] = "Remove the {mode} colour";
inline constexpr char kResetBothTitle[] = "Remove both colours";

// The names the reset row's script reads, as `var` statements for the
// selector script's opening (each begins with its `;`).
[[nodiscard]] inline std::string ResetNames() {
  std::string names =
      "; var resetClass = " + web_text::JsStringLiteral(kSelectorResetClass);
  names += "; var resetGlyph = " + web_text::JsStringLiteral(kResetGlyph);
  names +=
      "; var resetBothLabel = " + web_text::JsStringLiteral(kResetBothLabel);
  names += "; var resetOneTitle = " + web_text::JsStringLiteral(kResetOneTitle);
  names +=
      "; var resetBothTitle = " + web_text::JsStringLiteral(kResetBothTitle);
  names += "; var noColourText = " +
           web_text::JsStringLiteral(
               remix_colors::FormatColour(remix_colors::kNoColour));
  return names;
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

}  // namespace edgeplus::remix_selector_reset
