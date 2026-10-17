// Unit tests for the Gemini panel's layout (panel_layout.h). No Edge/Win32
// deps. Run via test\run.cmd.
#include <cstdio>
#include "../src/panel_layout.h"

using namespace fixedge;
using namespace fixedge::panel_layout;

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

bool HasBounds(const Rect& rect, int x, int y, int width, int height) {
  return rect.x == x && rect.y == y && rect.width == width &&
         rect.height == height;
}

void RunChecks() {
  const Rect content{0, 100, 1400, 800};
  const Layout collapsed = PanelLayout(content, false);
  Check(HasBounds(collapsed.strip, 1400 - kPanelWidth, 100, kPanelWidth,
                  kStripHeight),
        "the strip is a slim bar at the top right");
  Check(HasBounds(collapsed.page, 1400 - kPanelWidth, 100 + kStripHeight,
                  kPanelWidth, 800 - kStripHeight),
        "Gemini fills the rest under the strip");

  const Layout expanded = PanelLayout(content, true);
  Check(HasBounds(expanded.strip, 1400 - kPanelWidth, 100, kPanelWidth, 800),
        "an open picker covers the whole panel");
  Check(HasBounds(expanded.page, collapsed.page.x, collapsed.page.y,
                  collapsed.page.width, collapsed.page.height),
        "Gemini keeps its place under the picker");

  const Layout narrow = PanelLayout(Rect{10, 0, 300, 500}, false);
  Check(HasBounds(narrow.strip, 10, 0, 300, kStripHeight),
        "a narrow window gives the panel all of it");

  const Layout tiny = PanelLayout(Rect{0, 0, 300, 20}, false);
  Check(tiny.strip.height == 20 && tiny.page.height == 0,
        "a content area shorter than the strip never goes negative");
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
