// Unit tests for remix_selector.h.
#include <cstdio>
#include <string>
#include "../../../../src/theme/remix/pages/remix_selector.h"

using namespace edgeplus::remix_selector;
using edgeplus::remix_selector_reset::kSelectorResetClass;

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
  Check(Has(selector, kSelectorResetClass) &&
            Has(selector, "\"Remove the {mode} colour\"") &&
            Has(selector, "\"Remove both colours\"") &&
            Has(selector, "var noColourText = \"0,0\"") &&
            Has(selector, "aria-label") && Has(selector, "button[data-mode]"),
        "the selector offers resets of the selected mode and of both, to the "
        "wheel's centre, each with its full name");
  Check(Has(selector, kSelectorPreviewClass) && Has(selector, "\"Preview\"") &&
            Has(neutral,
                "[aria-checked=\"true\"][data-edge=\"false\"] "
                ".edgeplus-mode-preview { visibility: visible; }"),
        "the previewed segment, selected and not Edge's, shows a Preview "
        "sublabel as Edge's shows its own");
  Check(Has(selector, kSelectorStatusClass) && Has(selector, "\"status\"") &&
            Has(selector, "group.title") &&
            Has(neutral, ".edgeplus-mode-status { position: absolute;") &&
            Has(neutral, "clip-path: inset(50%)") &&
            !Has(selector, "edgeplus-mode-hint"),
        "the full preview sentence is the group's title and a visually hidden "
        "live region, never a line of its own in the column");
  Check(Has(neutral, "grid-row: 2; grid-column: 1; visibility: hidden;"),
        "the sublabels share one line, kept when hidden, so nothing moves");
  Check(Has(neutral, ".edgeplus-mode-reset {") &&
            Has(neutral, "button:disabled") &&
            Has(skinned, "button:not(:disabled):hover"),
        "the reset row is styled with and without the skin, its disabled "
        "buttons quiet");
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
