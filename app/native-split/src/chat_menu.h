// chat_menu.h — the chevron beside the chat button. One concern: find the
// dropdown button (EdgeCopilotDropdownMenuButton) that sits next to Edge's
// toolbar chat pill. UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::chat_menu {

using namespace fixedge::edge155;

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

}  // namespace fixedge::chat_menu
