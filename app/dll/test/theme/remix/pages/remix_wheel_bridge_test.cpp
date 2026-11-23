// Unit tests for remix_wheel_bridge.h, and that the colour selector
// (remix_selector.h) asks it in the same words.
#include <cstdio>
#include <string>
#include "../../../../src/theme/remix/pages/remix_selector.h"
#include "../../../../src/theme/remix/pages/remix_wheel_bridge.h"

using namespace edgeplus::remix_wheel_bridge;
using edgeplus::remix_selector::SelectorScript;

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

void RunChecks() {
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

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
