// Unit tests for reading_themes.h.
#include <cstdio>
#include <cstring>
#include "../src/theme/reading_themes.h"

using namespace edgeplus::reading_themes;

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

  const Choices defaults;
  Check(ForMode(defaults, false) == ReadingTheme::kSepia &&
            ForMode(defaults, true) == ReadingTheme::kBlack,
        "Sepia for light and Black for dark until something is picked");
  const Choices picked{ReadingTheme::kLavender, ReadingTheme::kDarkGrey};
  Check(ForMode(picked, true) == ReadingTheme::kDarkGrey,
        "dark Windows takes the dark choice");

  const auto bytes = BytesOf(picked);
  Check(bytes[0] == 18 && bytes[4] == 22 && bytes[1] == 0,
        "the file is two little-endian numbers, light first");
  Check(ChoicesFromBytes(bytes) == picked, "a file reads back as written");
  const uint8_t shortFile[3] = {18, 0, 0};
  Check(ChoicesFromBytes(shortFile) == defaults,
        "a file of another size reads as the defaults");
  const uint8_t badDark[8] = {18, 0, 0, 0, 7, 0, 0, 0};
  const Choices halfRead = ChoicesFromBytes(badDark);
  Check(halfRead.light == ReadingTheme::kLavender &&
            halfRead.dark == ReadingTheme::kBlack,
        "a side that is not one of the twelve keeps its default");

  Choices answered;
  Check(ChoicesFromFields("16", "3", &answered) &&
            answered.light == ReadingTheme::kSkyBlue &&
            answered.dark == ReadingTheme::kBlack,
        "the page's two numbers are read");
  Check(!ChoicesFromFields("16", "7", &answered) &&
            !ChoicesFromFields("x", "3", &answered) &&
            !ChoicesFromFields("16 ", "3", &answered) &&
            answered.light == ReadingTheme::kSkyBlue,
        "an answer that is not two of the twelve changes nothing");
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
