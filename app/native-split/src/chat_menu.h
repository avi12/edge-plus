// chat_menu.h — the title of the chat button's dropdown. One concern: the chevron
// beside the chat pill (EdgeCopilotDropdownMenuButton) is titled "Copilot menu"
// for its tooltip and accessible name, set once in its constructor. While Gemini
// is the chosen assistant it is retitled "Gemini menu", and put back to Edge's
// own string when Copilot is chosen again. UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::chat_menu {

using namespace fixedge::edge155;

inline constexpr char kGeminiMenuTitle[] = "Gemini menu";

// The chevron next to chat button `button`: a sibling in the same container.
inline void* DropdownBeside(void* button) {
  void* container = edge::ViewParent(button);
  if (!container) {
    return nullptr;
  }
  const size_t childCount = edge::ViewChildCount(container);
  for (size_t i = 0; i < childCount; i++) {
    void* child = edge::ViewChildAt(container, i);
    if (edge::IsCopilotDropdownButton(child)) {
      return child;
    }
  }
  return nullptr;
}

// Title the chevron beside `button` for the chosen assistant: Gemini's, or the
// string Edge gave it.
inline void Retitle(void* button, bool isGemini) {
  void* dropdown = DropdownBeside(button);
  if (!dropdown) {
    return;
  }
  if (isGemini) {
    edge::ViewSetTooltip(dropdown, kGeminiMenuTitle);
    edge::ViewSetAccessibleName(dropdown, kGeminiMenuTitle);
    return;
  }
  // "Copilot menu", built as Edge's constructor does. SetName takes its string
  // by value, so each call gets one of its own.
  edge::U16String copilot{};
  edge::LocalizedString(&copilot, off::COPILOT_NAME);
  edge::U16String tooltip{};
  edge::LocalizedString(&tooltip, off::COPILOT_MENU_TITLE_FORMAT, &copilot);
  edge::ViewSetTooltip(dropdown, &tooltip);
  edge::U16String name{};
  edge::LocalizedString(&name, off::COPILOT_MENU_TITLE_FORMAT, &copilot);
  edge::ViewSetAccessibleName(dropdown, &name);
}

}  // namespace fixedge::chat_menu
