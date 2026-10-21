// Unit tests for the reading mode's page themes (reading_themes.h). No
// Edge/Win32 deps. Run via test\run.cmd.
#include <cstdio>
#include <cstring>
#include "../src/reading_themes.h"

using namespace fixedge::reading_themes;

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
  Check(kThemes.size() == 12, "the grid offers twelve themes");
  for (size_t i = 0; i < kThemes.size(); i++) {
    for (size_t j = i + 1; j < kThemes.size(); j++) {
      Check(kThemes[i].theme != kThemes[j].theme &&
                std::strcmp(kThemes[i].name, kThemes[j].name) != 0,
            "each theme appears once, under its own name");
    }
  }
  Check(static_cast<int>(ReadingTheme::kSepia) == 0 &&
            static_cast<int>(ReadingTheme::kBlack) == 3 &&
            static_cast<int>(ReadingTheme::kDarkGrey) == 22,
        "the values are Edge's reading_view.theme numbers");
  Check(IsOffered(18) && !IsOffered(7) && !IsOffered(-1) && !IsOffered(23),
        "only the grid's twelve are offered (Rose, 7, is in Edge's table but "
        "not the grid)");

  ReadingTheme theme = ReadingTheme::kLight;
  Check(ThemeForMode(true, 0, 3, &theme) && theme == ReadingTheme::kBlack,
        "dark Windows takes the dark choice");
  Check(ThemeForMode(false, 0, 3, &theme) && theme == ReadingTheme::kSepia,
        "light Windows takes the light choice");
  Check(!ThemeForMode(true, 0, 7, &theme),
        "a choice that is not offered applies nothing");
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
