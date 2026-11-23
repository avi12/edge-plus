// assistant_button.h — the browser assistant's (Copilot's) toolbar button:
// finding it, telling it from its dropdown, and rebuilding it.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/api/views.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// The EdgeToolbarView that owns chat button `button`, or null: the ancestor
// whose chat-button field is the button.
inline void* ToolbarOfChatButton(void* button) {
  for (void* view = ViewParent(button); view; view = ViewParent(view)) {
    const void* field =
        reinterpret_cast<uint8_t*>(view) + edge155::off::EDGETOOLBAR_chatButton;
    const bool isOwner =
        IsReadable(field, sizeof(void*)) &&
        Field<void*>(view, edge155::off::EDGETOOLBAR_chatButton) == button;
    if (isOwner) {
      return view;
    }
  }
  return nullptr;
}

// Build `toolbar`'s chat button again, as Edge does for a new window (so the
// chevron's container if Edge now says so), and take the old one out of the
// view tree. The old button is detached, never deleted: other Edge objects may
// still point at it, and a view nobody draws is harmless. False when Edge built
// nothing.
inline bool RebuildChatButton(void* toolbar) {
  void* oldButton = Field<void*>(toolbar, edge155::off::EDGETOOLBAR_chatButton);
  void* oldParent = ViewParent(oldButton);
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::EdgeToolbarView_CreateDiscoverButtonIfNecessary)(
      reinterpret_cast<uint8_t*>(toolbar) +
      edge155::off::EDGETOOLBAR_discoverBase);
  const bool isRebuilt =
      Field<void*>(toolbar, edge155::off::EDGETOOLBAR_chatButton) != oldButton;
  if (isRebuilt && oldParent) {
    ViewRemoveChild(oldParent, oldButton);
  }
  return isRebuilt;
}

// Whether `view` is Edge's toolbar chat button (EdgeCopilotToolbarLabelButton).
inline bool IsCopilotButton(void* view) {
  const uintptr_t vtable = VtableRva(view);
  for (const uintptr_t candidate : edge155::rva::CopilotButton_vftables) {
    if (vtable == Rva(candidate)) {
      return true;
    }
  }
  return false;
}

// Whether `view` is the chevron beside the chat pill
// (EdgeCopilotDropdownMenuButton).
inline bool IsCopilotDropdownButton(void* view) {
  const uintptr_t vtable = VtableRva(view);
  for (const uintptr_t candidate :
       edge155::rva::CopilotDropdownButton_vftables) {
    if (vtable == Rva(candidate)) {
      return true;
    }
  }
  return false;
}

}  // namespace edgeplus::edge
