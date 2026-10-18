// Unit tests for the page menu's Gemini table (page_menu.h). No Edge/Win32 deps.
// Run via test\run.cmd.
#include <cstdio>
#include <string>
#include "../src/page_menu.h"

using namespace fixedge::page_menu;
using fixedge::chat_action::ChatAction;

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
  const Item* ask = ItemFor(static_cast<int>(Command::kAskSubmenu));
  Check(ask && ask->treatment == GeminiTreatment::kRelabel,
        "the Ask submenu is relabelled");
  Check(ask && std::string(ask->geminiLabel) == "Ask Gemini",
        "the Ask submenu reads Ask Gemini");

  const Item* selection = ItemFor(static_cast<int>(Command::kSelectionSubmenu));
  Check(selection && selection->treatment == GeminiTreatment::kRemove,
        "the selection submenu is removed");
  const Item* pdfSelection =
      ItemFor(static_cast<int>(Command::kSelectionSubmenuPdf));
  Check(pdfSelection && pdfSelection->treatment == GeminiTreatment::kRemove,
        "the PDF selection submenu is removed");

  Check(ItemFor(0) == nullptr, "an unknown command has no row");
  Check(ItemFor(static_cast<int>(Command::kPodcast)) == nullptr,
        "the podcast keeps Edge's text");

  Check(ActionFor(static_cast<int>(Command::kSummarize)) ==
            ChatAction::kSummarize,
        "Summarize runs the summary");
  Check(ActionFor(static_cast<int>(Command::kPodcast)) == ChatAction::kPodcast,
        "Create podcast runs the podcast");
  Check(ActionFor(static_cast<int>(Command::kAskSubmenu)) == ChatAction::kNone,
        "the submenu itself runs nothing");
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
