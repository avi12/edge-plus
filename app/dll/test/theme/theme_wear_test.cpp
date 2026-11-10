// Unit tests for theme_wear.h.
#include <cstdio>
#include "../../src/theme/theme_wear.h"

using namespace edgeplus::theme_wear;
using edgeplus::theme_notice::Assigned;

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
constexpr uint64_t kStart = 1000;

void RunChecks() {
  Check(CauseOfDefault(false, false) == Cause::kUser,
        "a default nobody at Edge+ asked for is the user's");
  Check(CauseOfDefault(true, false) == Cause::kEdgePlus,
        "Edge+'s own UseDefaultTheme is Edge+'s");
  Check(CauseOfDefault(false, true) == Cause::kEdgePlus,
        "a drop to default during Edge+'s switch is Edge+'s");

  const OwnSwitch idle;
  Check(!idle.IsSettling(kStart), "no switch, nothing settling");

  OwnSwitch building;
  building.Started(kDark, kStart);
  Check(building.IsSettling(kStart + 1), "a switch settles until built");
  building.Built(kLight);
  Check(building.IsSettling(kStart + 2), "another theme built does not end it");
  building.Built(kDark);
  Check(!building.IsSettling(kStart + 3), "the asked theme built ends it");

  OwnSwitch neverBuilt;
  neverBuilt.Started(kDark, kStart);
  Check(neverBuilt.IsSettling(kStart + kSwitchSettleMs - 1),
        "a switch settles for a while");
  Check(!neverBuilt.IsSettling(kStart + kSwitchSettleMs),
        "a theme never built stops muting the user");

  OwnSwitch toDefault;
  toDefault.Started({}, kStart);
  Check(!toDefault.IsSettling(kStart + 1),
        "a switch to the default has nothing to wait for");

  const Assigned active{kLight, kDark, true};
  const Assigned inactive{kLight, kDark, false};
  Check(CauseOfTheme(active, kDark) == Cause::kEdgePlus,
        "an assigned theme is Edge+'s");
  Check(CauseOfTheme(active, kStoreTheme) == Cause::kUser,
        "a store theme is the user's");
  Check(CauseOfTheme(inactive, kDark) == Cause::kUser,
        "with theming off every theme is the user's");
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
