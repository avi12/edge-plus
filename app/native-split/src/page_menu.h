// page_menu.h — the page's right-click "Ask Copilot" items, with Gemini chosen.
// One concern, pure (no Edge, no Win32; unit-tested in test/page_menu_test.cpp):
// the command ids Edge gives those items, and the one table saying what each
// becomes while Gemini is the assistant. The hooks that apply it are
// assistant.h's, the menu surgery is page_menu_edit.h's.
#pragma once
#include <cstddef>
#include "chat_action.h"

namespace fixedge::page_menu {

// The command ids of RenderViewContextMenu's Copilot items (IDC_...).
enum class Command : int {
  kAskSubmenu =
      0xcd76,  // "Ask Copilot": the submenu on a page with no selection
  kSummarize = 0xcd71,  // "Summarize" (a lone "Summarize with Copilot" in L1)
  kPodcast = 0xcd77,    // "Create podcast"
  kSelectionSubmenu = 0xcd68,     // "Ask Copilot" with a selection
  kSelectionSubmenuPdf = 0xcd69,  // the same on a PDF
};

// What an item becomes while Gemini is chosen.
enum class GeminiTreatment {
  kRelabel,  // keep, with Gemini's text
  kRemove,   // drop: its Copilot-only items would open Copilot's pane
};

struct Item {
  Command command;
  GeminiTreatment treatment;
  const char* geminiLabel;  // for kRelabel
};

// The items of the menu's own model (the top level), by id. The submenu's
// children (Summarize, Create podcast) keep Edge's text: it names no assistant.
inline constexpr Item kItems[] = {
    {Command::kAskSubmenu, GeminiTreatment::kRelabel, "Ask Gemini"},
    {Command::kSummarize, GeminiTreatment::kRelabel, "Summarize with Gemini"},
    {Command::kSelectionSubmenu, GeminiTreatment::kRemove, nullptr},
    {Command::kSelectionSubmenuPdf, GeminiTreatment::kRemove, nullptr},
};

// The table row for `commandId`, or null when the item is not ours.
inline const Item* ItemFor(int commandId) {
  for (const Item& item : kItems) {
    if (static_cast<int>(item.command) == commandId) {
      return &item;
    }
  }
  return nullptr;
}

// What Gemini runs when the item is chosen; kNone for any other command.
inline chat_action::ChatAction ActionFor(int commandId) {
  if (commandId == static_cast<int>(Command::kSummarize)) {
    return chat_action::ChatAction::kSummarize;
  }
  if (commandId == static_cast<int>(Command::kPodcast)) {
    return chat_action::ChatAction::kPodcast;
  }
  return chat_action::ChatAction::kNone;
}

}  // namespace fixedge::page_menu
