// Unit tests for tab_edge.h.
#include <cstdio>
#include "../../../src/chrome/tabs/tab_edge.h"

using namespace edgeplus;
using namespace edgeplus::tab_edge;

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
  Check(LeadingSide(false) == Side::kLeft && LeadingSide(true) == Side::kRight,
        "left to right leads on the left, right to left on the right");

  const CornerRadii square{};
  bool isAllSquare = true;
  for (const CornerRadius& corner : square.corners) {
    isAllSquare = isAllSquare && corner == CornerRadius{};
  }
  Check(isAllSquare, "radii left unset square every corner");

  Check(
      LeadingStrip(Rect{4, 0, 240, 32}, Side::kLeft, 7) == Rect{4, 0, 7, 32} &&
          LeadingStrip(Rect{4, 0, 240, 32}, Side::kRight, 7) ==
              Rect{237, 0, 7, 32},
      "the corner strip hugs the leading side");
  Check(LeadingStrip(Rect{0, 0, 5, 32}, Side::kLeft, 7) == Rect{0, 0, 5, 32},
        "a narrow rect is covered whole, not overshot");
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
