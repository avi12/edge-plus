// assistant.h — the browser assistant: Copilot. One concern: keep the chevron
// beside Edge's toolbar chat button, whose dropdown carries the chat menu
// (Summarize, Talk). A personal profile in Edge's Copilot Mode gets the button
// without it, so one hook answers "the dropdown is enabled" and the toolbars
// already built are rebuilt to grow it. The hook is independent of the split,
// so turning the split off does not take the chevron away.
#pragma once
#include <vector>
#include "MinHook.h"
#include "chat_menu.h"
#include "chat_target.h"
#include "edge.h"

namespace fixedge::assistant {

// The assistant's name, for every place the UI mentions it (the enhanced
// find's "Ask <name>"); the table is chat_target.h's.
inline const char* Name() {
  return chat_target::kCopilotTarget.name;
}

// A personal profile in Edge's Copilot Mode gets the chat button without its
// chevron: Edge answers "no" here when the toolbar is built. The chevron
// carries the chat menu, so it is always wanted: this answers "yes".
// Takes effect for each toolbar built after the DLL loads (a new window);
// EnsureChevron rebuilds the ones already open.
using DropdownEnabled_t = bool(__fastcall*)(bool);
inline DropdownEnabled_t o_dropdown_enabled = nullptr;

inline bool __fastcall h_dropdown_enabled(bool /*isChecked*/) {
  return true;
}

// --- the chevron on the live button --------------------------------------

// Collect every chat button under `view` (one per browser window; usually one).
inline void CollectButtons(void* view, std::vector<void*>* buttons) {
  if (edge::IsCopilotButton(view)) {
    buttons->push_back(view);
  }
  const size_t childCount = edge::ViewChildCount(view);
  for (size_t i = 0; i < childCount; i++) {
    CollectButtons(edge::ViewChildAt(view, i), buttons);
  }
}

// A toolbar built before this DLL loaded has the bare chat button: build it again
// so it gets the split-button container and its chevron.
inline void EnsureChevron(void* mcv) {
  std::vector<void*> buttons;
  CollectButtons(edge::ViewRoot(mcv), &buttons);
  for (void* button : buttons) {
    void* dropdown = chat_menu::DropdownBeside(button);
    if (dropdown) {
      edge::SetViewVisible(dropdown, true);
    } else {
      void* toolbar = edge::ToolbarOfChatButton(button);
      const bool isRebuilt = toolbar && edge::RebuildChatButton(toolbar);
      Log("[assistant] chat button rebuilt with chevron: %d",
          static_cast<int>(isRebuilt));
    }
    // The toolbar is not above the page in the view tree: its own layout is
    // told, so the chevron gets its room.
    for (void* view = button; view; view = edge::ViewParent(view)) {
      edge::InvalidateLayout(view);
    }
  }
}

// --- install / apply -----------------------------------------------------

inline bool EnableAll() {
  const MH_STATUS status = MH_EnableHook(
      edge::At(edge155::rva::Copilot_IsConsumerLabelButtonWithDropdownEnabled));
  return status == MH_OK || status == MH_ERROR_ENABLED;
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(
          edge::At(
              edge155::rva::Copilot_IsConsumerLabelButtonWithDropdownEnabled),
          reinterpret_cast<void*>(h_dropdown_enabled),
          reinterpret_cast<void**>(&o_dropdown_enabled)) == MH_OK;
  if (!isHooked) {
    Log("[err] assistant hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] assistant hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

// Give `mcv`'s window the chevron now, and lay the whole window out again so the
// toolbar settles around it.
inline void Apply(void* mcv) {
  if (!mcv) {
    return;
  }
  EnsureChevron(mcv);
  for (void* view = mcv; view; view = edge::ViewParent(view)) {
    edge::InvalidateLayout(view);
  }
  edge::LayoutImmediately(edge::ViewRoot(mcv));
}

}  // namespace fixedge::assistant
