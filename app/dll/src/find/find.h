// find.h — the enhanced find, through Edge's own find bar, which stays the only
// UI. A RegExp, or a question for the browser assistant (Copilot) about which
// paragraphs answer it, is searched by us instead of Edge; anything else is
// Edge's own search, untouched. Matches are drawn with the CSS Custom Highlight
// API (find_scripts.h), so the page's DOM is not touched, and the count and any
// status go in the bar's own label. This header installs the detours
// (find_detours.h) and turns the feature on and off; its parts are the other
// find_*.h headers.
#pragma once
#include <windows.h>
#include <vector>
#include "MinHook.h"
#include "assistant/assistant_chat.h"
#include "core/channel.h"
#include "core/log.h"
#include "edge/api/binding.h"
#include "edge/offsets.h"
#include "find/find_bar_session.h"
#include "find/find_bar_width.h"
#include "find/find_detours.h"
#include "find/find_options.h"
#include "find/find_poll.h"
#include "find/find_session.h"
#include "find/find_status.h"
#include "find/find_text.h"
#include "find/find_toggles.h"

namespace edgeplus::find {

struct HookSite {
  uintptr_t rva;
  void* detour;
  void** original;
};

inline std::vector<HookSite> HookSites() {
  return {
      {edge155::rva::FindBarView_Find, reinterpret_cast<void*>(h_find),
       reinterpret_cast<void**>(&o_find)},
      {edge155::rva::FindBarView_FindNext, reinterpret_cast<void*>(h_find_next),
       reinterpret_cast<void**>(&o_find_next)},
      {edge155::rva::FindBarView_HandleKeyEvent,
       reinterpret_cast<void*>(h_handle_key_event),
       reinterpret_cast<void**>(&o_handle_key_event)},
      {edge155::rva::FindBarController_EndFindSession,
       reinterpret_cast<void*>(h_end_find_session),
       reinterpret_cast<void**>(&o_end_find_session)},
      {edge155::rva::FindBarView_EdgeMaybeAddFindOptionsView,
       reinterpret_cast<void*>(h_add_options_view),
       reinterpret_cast<void**>(&o_add_options_view)},
      {edge155::rva::FindBarView_EdgeToggleFindOptions,
       reinterpret_cast<void*>(h_toggle_options),
       reinterpret_cast<void**>(&o_toggle_options)},
      {edge155::rva::FindInPageOptionToggleView_OnButtonToggled,
       reinterpret_cast<void*>(h_button_toggled),
       reinterpret_cast<void**>(&o_button_toggled)},
      {edge155::rva::FindInPageOptionToggleView_EdgeOnActiveWebContentsChanged,
       reinterpret_cast<void*>(h_active_contents_changed),
       reinterpret_cast<void**>(&o_active_contents_changed)},
  };
}

// Re-enabled after the split's MH_ALL_HOOKS turns everything off; with the find
// switched off they pass through.
inline bool EnableAll() {
  bool isEnabled = true;
  for (const HookSite& site : HookSites()) {
    const MH_STATUS status = edge::EnableHookAt(site.rva);
    isEnabled = isEnabled && (status == MH_OK || status == MH_ERROR_ENABLED);
  }
  return isEnabled;
}

inline bool Install() {
  bool isHooked = true;
  for (const HookSite& site : HookSites()) {
    isHooked = isHooked && MH_CreateHook(edge::At(site.rva), site.detour,
                                         site.original) == MH_OK;
  }
  if (!isHooked) {
    Log("[err] find hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] find hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

inline void Tick() {
  DropStale();
  for (auto& [mcv, session] : Sessions()) {
    const bool isBarOpen =
        (session.findBarView != nullptr) && session.widenedBarWidth != 0;
    if (isBarOpen) {
      ApplyBarWidth(session,
                    OptionOf(session.findBarView) == find_text::Option::kAsk);
    }
  }
  TurnSpinners();
  assistant_chat::Tick();
  StopTimerIfIdle();
}

inline void RemoveAll() {
  DropStale();
  std::vector<void*> windows;
  for (const auto& [mcv, session] : Sessions()) {
    windows.push_back(mcv);
  }
  for (void* mcv : windows) {
    Close(mcv);
  }
  assistant_chat::RemoveAll();
  Sessions().clear();
  if (channel::g_window) {
    KillTimer(channel::g_window, kTimerId);
  }
}

inline void Apply(bool isEnabled) {
  const bool isTurningOff = g_isEnabled && !isEnabled;
  const bool isChanging = g_isEnabled != isEnabled;
  if (isChanging) {
    Log("[find] switched %s", isEnabled ? "on" : "off");
  }
  DropStale();
  std::vector<void*> bars;
  for (const auto& [mcv, session] : Sessions()) {
    if (session.findBarView) {
      bars.push_back(session.findBarView);
    }
  }
  g_isEnabled = isEnabled;
  if (isTurningOff) {
    RemoveAll();
  }
  if (isChanging) {
    for (void* findBarView : bars) {
      SyncToggles(findBarView);
    }
  }
}

}  // namespace edgeplus::find
