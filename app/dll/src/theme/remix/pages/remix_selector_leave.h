// remix_selector_leave.h — the Appearance editor's colour selector
// (remix_selector.h) puts Edge's mode back before the user leaves the
// section or the editor while previewing the other one. Pure: strings only.
#pragma once
#include <string>

namespace edgeplus::remix_selector_leave {

// Leaving while previewing the other mode: Edge's mode is applied first, and
// the Back or Close goes through this long after.
inline constexpr int kRestoreDelayMs = 500;

// The names the leave guard's script reads, as `var` statements for the
// selector script's opening (each begins with its `;`).
[[nodiscard]] inline std::string LeaveNames() {
  return "; var restoreDelay = " + std::to_string(kRestoreDelayMs);
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

}  // namespace edgeplus::remix_selector_leave
