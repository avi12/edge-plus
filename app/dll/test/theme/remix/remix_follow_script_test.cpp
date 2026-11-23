// Unit tests for remix_follow_script.h.
#include <cstdio>
#include <string>
#include "../../../src/theme/remix/remix_follow_script.h"

using edgeplus::remix_follow_script::FollowModeScript;
using edgeplus::remix_page_helper::kHelperScript;

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
  edgeplus::remix_colors::Pair pair;
  pair.dark = {228, 20, true};
  const std::string script = FollowModeScript(true, pair);
  Check(script.starts_with(
            "(() => {\n  if (!globalThis.__blueprintParams) { return; }\n  (" +
            std::string(kHelperScript) + ")({light: null, dark: [228, 20]})"),
        "only a page wearing an Appearance remix installs the helper, with "
        "the saved pair");
  Check(script.ends_with(".push(true, \"applyColorWheelThemeV2\");\n})();"),
        "and pushes the target mode to it, as one self-contained call");
  Check(
      FollowModeScript(false, pair).find(".push(false, ") != std::string::npos,
      "a light flip pushes light");
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
