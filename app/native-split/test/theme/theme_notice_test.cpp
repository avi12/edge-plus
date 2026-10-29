// Unit tests for theme_notice.h.
#include <cstdio>
#include "../../src/theme/theme_notice.h"

using namespace edgeplus::theme_notice;

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

constexpr std::string_view kLight = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr std::string_view kDark = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr std::string_view kStoreTheme = "cccccccccccccccccccccccccccccccc";

void RunChecks() {
  const Assigned active{kLight, kDark, true};
  const Assigned inactive{kLight, kDark, false};
  const Assigned unassigned{{}, {}, true};

  Check(ShouldSuppress(active, kLight, false), "the light theme is Edge+'s");
  Check(ShouldSuppress(active, kDark, false), "the dark theme is Edge+'s");
  Check(!ShouldSuppress(active, kStoreTheme, false),
        "a theme from a store keeps its bubble");
  Check(!ShouldSuppress(active, "", false), "no id is nobody's theme");
  Check(!ShouldSuppress(inactive, kLight, false),
        "with theming off the same id is the user's own install");
  Check(!ShouldSuppress(unassigned, "", false),
        "an empty assignment does not match an empty id");
  Check(!ShouldSuppress(unassigned, kLight, false),
        "nothing assigned, nothing suppressed");
  Check(ShouldSuppress(inactive, kStoreTheme, true),
        "Edge's own request for no bubble is never undone");
  Check(!ShouldSuppress(active, "aaaaaaaa", false),
        "a prefix of an assigned id is another theme");
  Check(!ShouldSuppress(active, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", false),
        "an assigned id with extra characters is another theme");
}

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (const CheckFailed&) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
