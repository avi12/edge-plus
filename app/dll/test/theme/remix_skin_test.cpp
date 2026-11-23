// Unit tests for remix_skin.h.
#include <cstdio>
#include <string>
#include "../../src/theme/remix_skin.h"

using namespace edgeplus::remix_skin;
using edgeplus::theme_tone::Tone;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

bool Has(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

void RunChecks() {
  Check(
      CssColour(0xff05080c) == "#05080c" && CssColour(0xfff72944) == "#f72944",
      "an opaque colour is its hex");
  Check(CssColour(0x38007979) == "rgb(0 121 121 / 0.219)",
        "a colour with alpha is rgb() with a slash alpha");

  const std::string dark = StyleFor(Tone::kDark);
  const std::string light = StyleFor(Tone::kLight);
  Check(Has(dark, "--ep-panel: #05080c;") &&
            Has(dark, "--ep-frame: #f72944;") &&
            Has(dark, "color-scheme: dark;"),
        "dark: the panel and red frame come from the palette");
  Check(Has(light, "--ep-frame: #06182b;") &&
            Has(light, "--ep-accent: #c8102e;") &&
            Has(light, "color-scheme: light;"),
        "light: ink frames, the deeper red marks");
  Check(!Has(dark, "--ep-on-fill: #56feff;;") &&
            Has(dark, "--ep-on-fill: #56feff;"),
        "cyan is only exposed as a fill role");
  Check(dark != light, "the two tones differ");

  for (const RoleVariable& variable : kRoleVariables) {
    Check(Has(dark, std::string(variable.name) + ": "),
          "every role variable is emitted");
  }
  for (const PageToken& token : kPageTokens) {
    Check(Has(dark, std::string(token.name) + ": var(" +
                        std::string(token.roleVariable) + ");"),
          "every page token is pointed at a role");
    bool isDeclared = false;
    for (const RoleVariable& variable : kRoleVariables) {
      isDeclared = isDeclared || variable.name == token.roleVariable;
    }
    Check(isDeclared, "a page token points only at a declared role variable");
  }

  // The structure holds no colour of its own: roles are the only colours.
  Check(std::string(kStructureCss).find('#') == std::string::npos,
        "the structure names no hex colour");
  Check(Has(kStructureCss, "clip-path: polygon(7px 0"),
        "controls are chamfered");

  Check(
      ShouldSkin(true, true) && !ShouldSkin(true, false) &&
          !ShouldSkin(false, true) && !ShouldSkin(false, false),
      "the pages are skinned only with the remix switch and the skin both on");

  const std::string neutral = SelectorStyleFor(false);
  const std::string skinned = SelectorStyleFor(true);
  Check(Has(neutral, ".lights-button") && !Has(neutral, "--ep-") &&
            Has(skinned, "--ep-accent") && skinned.size() > neutral.size(),
        "the selector is neutral without the skin and takes its roles with it");
  const std::string selector = SelectorScript(false, "light=1,2;dark=");
  Check(Has(selector, "light=1,2;dark=") && Has(selector, "pointerdown") &&
            Has(selector, "isTouched") && Has(selector, "wasShown") &&
            Has(selector, "edgeMode()") && Has(selector, "replaceState") &&
            Has(selector, "#edgeplus-remix=") &&
            Has(SelectorRemoveScript(), ".remove()"),
        "the selector seeds the pair, drives the wheel and publishes the pair");

  const std::string paint = PaintScript(Tone::kDark);
  Check(Has(paint, kStyleClass) && Has(paint, "shadowRoot") &&
            Has(paint, "#05080c"),
        "the paint script carries the style into every shadow root");
  const std::string remove = RemoveScript();
  Check(Has(remove, kStyleClass) && Has(remove, ".remove()"),
        "the remove script takes the styles away");
}

}  // namespace

void RunWheelBridgeChecks() {
  const std::string bridge = WheelBridgeScript();
  const std::string selector = SelectorScript(false, "light=270,60;dark=");
  Check(bridge.find(kWheelEventName) != std::string::npos &&
            selector.find(kWheelEventName) != std::string::npos,
        "the bridge and the editor script name the same wheel event");
  Check(bridge.find(kWheelAttribute) != std::string::npos &&
            selector.find(kWheelAttribute) != std::string::npos,
        "and the same attribute");
  Check(bridge.find("setKnobFromHueIntensity") != std::string::npos &&
            bridge.find("applyThemeCustomizer") == std::string::npos,
        "the bridge sets the knob and never applies");
  Check(bridge.find(kArtifactEventName) != std::string::npos &&
            selector.find(kArtifactEventName) != std::string::npos &&
            bridge.find(kArtifactAttribute) != std::string::npos &&
            selector.find(kArtifactAttribute) != std::string::npos,
        "the bridge and the editor script name the same artifact question");
  Check(bridge.find("artifactUid") != std::string::npos &&
            selector.find("isNew") != std::string::npos &&
            selector.find("isTrusted") != std::string::npos,
        "the bridge answers with the component's artifactUid, and the editor "
        "tells a new appearance and takes only the user's own picks");
  const std::string bridgeRemove = WheelBridgeRemoveScript();
  Check(bridge.find(kWheelBridgeKey) != std::string::npos &&
            bridgeRemove.find(kWheelBridgeKey) != std::string::npos &&
            bridgeRemove.find("removeEventListener") == std::string::npos &&
            bridge.find("removeEventListener") != std::string::npos,
        "the bridge keeps a remover under its key, which unloading calls");
}
int main() {
  try {
    RunChecks();
    RunWheelBridgeChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
