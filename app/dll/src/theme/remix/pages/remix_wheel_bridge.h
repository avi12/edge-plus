// remix_wheel_bridge.h — the colour selector's (remix_selector.h) helper in
// the Appearance editor page's own world: it sets the wheel's knob without
// an apply and says what the opened section edits. Pure: strings only.
#pragma once
#include <string>
#include "find/web_text.h"

namespace edgeplus::remix_wheel_bridge {

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

// The names both sides of the bridge ask and answer by, as the `var`
// statements that open the bridge and the editor script (remix_selector.h)
// alike, so the two can never name them apart.
[[nodiscard]] inline std::string SharedNames() {
  std::string names =
      "var wheelEvent = " + web_text::JsStringLiteral(kWheelEventName);
  names +=
      "; var wheelAttribute = " + web_text::JsStringLiteral(kWheelAttribute);
  names +=
      "; var artifactEvent = " + web_text::JsStringLiteral(kArtifactEventName);
  names += "; var artifactAttribute = " +
           web_text::JsStringLiteral(kArtifactAttribute);
  return names;
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
  std::string script = "(function () { " + SharedNames();
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

}  // namespace edgeplus::remix_wheel_bridge
