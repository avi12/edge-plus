// assistant.h — the name of the assistant Enhanced Find asks, and the removal
// of the chevron an earlier Edge+ added beside Edge's toolbar chat button.
#pragma once
#include <vector>
#include "assistant/chat_menu.h"
#include "assistant/chat_target.h"
#include "edge/edge.h"

namespace edgeplus::assistant {

inline const char* Name() {
  return chat_target::kCopilotTarget.name;
}

inline void CollectButtons(void* view, std::vector<void*>* buttons) {
  if (edge::IsCopilotButton(view)) {
    buttons->push_back(view);
  }
  const size_t childCount = edge::ViewChildCount(view);
  for (size_t i = 0; i < childCount; i++) {
    CollectButtons(edge::ViewChildAt(view, i), buttons);
  }
}

// Hides a chevron left by an Edge+ that forced it on, so a window opened
// before this build loses it without a restart.
inline void RemoveChevron(void* mcv) {
  if (!mcv) {
    return;
  }
  std::vector<void*> buttons;
  CollectButtons(edge::ViewRoot(mcv), &buttons);
  for (void* button : buttons) {
    // Rebuilding adds a new button to the tree; the old one stays only while
    // its chevron is still beside it, so a second pass finds nothing to do.
    void* dropdown = chat_menu::DropdownBeside(button);
    if (dropdown) {
      edge::SetViewVisible(dropdown, false);
      // The pill keeps the flat right edge it had beside the chevron; Edge
      // builds it rounded when it builds it alone.
      void* toolbar = edge::ToolbarOfChatButton(button);
      const bool isRebuilt =
          (toolbar != nullptr) && edge::RebuildChatButton(toolbar);
      Log("[assistant] chat button rebuilt without chevron: %d",
          static_cast<int>(isRebuilt));
    }
    for (void* view = button; view; view = edge::ViewParent(view)) {
      edge::InvalidateLayout(view);
    }
  }
}

}  // namespace edgeplus::assistant
