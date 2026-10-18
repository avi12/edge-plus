// chat_actions.h — the chat button's dropdown, with Gemini chosen. One concern:
// Edge's menu (EdgeCopilotMenuModel: "Summarize page", "Chat", "Talk to Copilot
// + Share tab") is rebuilt every time it opens, so while Gemini is the assistant
// its voice item is renamed after the build, and its items are run here (in the
// Gemini panel) instead of by Edge (in Copilot's). The hooks that call these are
// assistant.h's. UI thread only.
#pragma once
#include <windows.h>
#include <cstring>
#include "chat_action.h"
#include "edge.h"
#include "gemini.h"
#include "page_menu.h"
#include "share.h"

namespace fixedge::chat_actions {

using namespace fixedge::edge155;

// Rename the voice item of a freshly built menu `model` after Gemini. The item
// is looked up by its command id; a menu that has none (Edge leaves it out in
// some states) is left alone.
inline void RelabelTalkItem(void* model) {
  const size_t count = edge::MenuItemCount(model);
  for (size_t i = 0; i < count; i++) {
    const bool isTalkItem =
        edge::MenuCommandIdAt(model, i) ==
        static_cast<int>(chat_action::MenuCommand::kTalkAndShare);
    if (isTalkItem) {
      edge::U16String label{};
      edge::Utf8ToUtf16(&label, chat_action::kTalkLabel);
      edge::MenuSetLabel(model, i, &label);
      return;
    }
  }
}

// Set the "Hide Copilot" item of chat button `button`'s right-click menu: after
// Gemini when `isGemini`, else Edge's own string. A button without that menu or
// item is left alone.
inline void RelabelHideItem(void* button, bool isGemini) {
  void* model = edge::ChatButtonContextMenuModel(button);
  if (!model) {
    return;
  }
  const size_t count = edge::MenuItemCount(model);
  for (size_t i = 0; i < count; i++) {
    const int commandId = edge::MenuCommandIdAt(model, i);
    const bool isHideItem = commandId == off::COPILOT_HIDE_COMMAND ||
                            commandId == off::COPILOT_HIDE_SETTINGS_COMMAND;
    if (isHideItem) {
      edge::U16String label{};
      if (isGemini) {
        edge::Utf8ToUtf16(&label, chat_action::kHideLabel);
      } else {
        edge::LocalizedString(&label, off::COPILOT_HIDE_LABEL);
      }
      edge::MenuSetLabel(model, i, &label);
      return;
    }
  }
}

// Make a freshly built page context menu Gemini's: each of its Copilot items
// (page_menu.h's table) is relabelled or removed. Walks from the end so a
// removal does not shift the items still to visit.
inline void ApplyPageMenu(void* contextMenu) {
  void* model = edge::PageContextMenuModel(contextMenu);
  for (size_t i = edge::MenuItemCount(model); i > 0; i--) {
    const size_t index = i - 1;
    const page_menu::Item* item =
        page_menu::ItemFor(edge::MenuCommandIdAt(model, index));
    if (!item) {
      continue;
    }
    if (item->treatment == page_menu::GeminiTreatment::kRemove) {
      edge::MenuRemoveItemAt(model, index);
      continue;
    }
    edge::U16String label{};
    const edge::U8View text{item->geminiLabel, std::strlen(item->geminiLabel)};
    edge::Utf8ToUtf16(&label, &text);
    edge::MenuSetLabel(model, index, &label);
  }
}

// Run a menu item in the Gemini panel of `window`: open the panel, then, for a
// summary or a talk, share the active tab and do what was asked with it. False
// when there is no panel to open (the caller then lets Edge run the item).
inline bool Run(HWND window, void* mcv, chat_action::ChatAction action) {
  if (!mcv || !gemini::EnsureOpen(window, mcv)) {
    return false;
  }
  share::StartTimer(channel::g_window);
  share::RequestAction(mcv, action);
  return true;
}

}  // namespace fixedge::chat_actions
