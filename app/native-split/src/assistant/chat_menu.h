// chat_menu.h — the chevron (EdgeCopilotDropdownMenuButton) beside Edge's
// toolbar chat button. UI thread only.
#pragma once
#include "edge/edge.h"

namespace edgeplus::chat_menu {

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

}  // namespace edgeplus::chat_menu
