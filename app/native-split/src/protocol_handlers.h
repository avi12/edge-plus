// protocol_handlers.h — "ask to handle links, like Chrome". One concern: a
// site's request to handle a link type (navigator.registerProtocolHandler, the
// way Gmail asks for mailto:) is only ever blocked when the user says so.
//
// Edge already shows Chrome's omnibox icon and bubble for a request made without
// a user gesture, and Edge's own prompt for one made with a gesture
// (docs/re/protocol-handlers.md). Both silently block the site for good on a
// non-answer: the bubble commits its preselected "Ignore" however it closes (Esc,
// its X, a click elsewhere), and the prompt's "Just this time" lands in the same
// ignored list as "Block". An ignored site's later requests are dropped without
// a trace, so the icon never comes back. This module makes:
//   - the bubble act only on Done; any other close leaves everything as it was,
//     the icon included, so the user can still answer later;
//   - the prompt's "Just this time" remember nothing, rather than block;
//   - every block already on record lifted whenever the feature turns on (at
//     load with it on, or when the panel switches it on), so a site blocked by
//     an old non-answer can ask again. Edge 155 has no settings page that
//     lists them.
// Allow, Deny, an explicit Ignore + Done, and the prompt's Block keep Edge's
// behaviour, until the feature next turns on.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "MinHook.h"
#include "config.h"
#include "edge.h"
#include "log.h"

namespace fixedge::protocol_handlers {

// The decisions Edge's permission prompt reports (Chromium's
// PermissionDecision). PermissionDecided accepts kAllow and ignores the rest.
enum class Decision : int32_t {
  kAllow = 0,
  kAllowThisTime = 1,  // "Just this time"
  kDeny = 2,           // "Block"
};

inline constinit bool g_isActive = false;

// The bubble model whose Done button was just pressed. Done runs the accept
// callback first and only then closes the bubble (CommitChanges), all on the UI
// thread, so the next CommitChanges for this model is Done's.
inline constinit void* g_doneModel = nullptr;

// Our OnDoneButtonClicked, installed in the handler bubble model's vtable slot
// while the feature is on (the slot's own function is an empty `ret`).
inline void __fastcall OnDoneButtonClicked(void* model) {
  g_doneModel = model;
}

// ContentSettingRPHBubbleModel::CommitChanges — run Edge's commit only when the
// bubble is closing because the user pressed Done.
using CommitChanges_t = void(__fastcall*)(void*);
inline constinit CommitChanges_t o_commit_changes = nullptr;
inline void __fastcall h_commit_changes(void* model) {
  const bool isDone = model == g_doneModel;
  g_doneModel = nullptr;
  const bool shouldCommit = !g_isActive || isDone;
  if (shouldCommit) {
    o_commit_changes(model);
    return;
  }
  Log("[handlers] bubble closed without Done; nothing remembered");
}

// RegisterProtocolHandlerPermissionRequest::PermissionDecided — "Just this
// time" decides nothing instead of blocking the site.
using PermissionDecided_t = void(__fastcall*)(void*, const void*);
inline constinit PermissionDecided_t o_permission_decided = nullptr;
inline void __fastcall h_permission_decided(void* request,
                                            const void* decision) {
  const edge::PermissionDecision decided =
      edge::ReadPermissionDecision(decision);
  Log("[handlers] prompt decided has=%d value=%d", decided.hasValue,
      decided.value);
  const bool isJustThisTime =
      g_isActive && decided.hasValue &&
      decided.value == static_cast<int32_t>(Decision::kAllowThisTime);
  if (isJustThisTime) {
    Log("[handlers] prompt answered just this time; nothing remembered");
    return;
  }
  o_permission_decided(request, decision);
}

// Lift every block in `profile`'s ignored list; returns how many were lifted.
// RemoveIgnoredHandler erases from that very list, so each handler is first
// copied out (Edge's copy constructor into storage we own), then removed by
// its copy, then destroyed. A profile with several windows is visited once per
// window; the later visits find nothing left to lift. A policy block stays
// (Edge keeps it in the live list), and is not counted.
inline size_t LiftIgnoredHandlers(void* profile) {
  void* registry =
      profile ? edge::ProtocolHandlerRegistryFor(profile) : nullptr;
  if (!registry) {
    return 0;
  }
  const size_t ignoredCount = edge::IgnoredHandlerCount(registry);
  if (ignoredCount == 0) {
    return 0;
  }
  std::vector<edge::ProtocolHandlerStorage> copies(ignoredCount);
  for (size_t i = 0; i < ignoredCount; i++) {
    edge::CopyIgnoredHandler(registry, i, &copies[i]);
  }
  for (edge::ProtocolHandlerStorage& copy : copies) {
    edge::RemoveIgnoredHandler(registry, &copy);
    edge::DestroyProtocolHandler(&copy);
  }
  return ignoredCount - edge::IgnoredHandlerCount(registry);
}

// Switch the hooks on or off with the feature. The two detours are created once
// (hooks.h) and pass straight through while the feature is off; only the Done
// slot is written and restored here.
inline void SetActive(bool isActive) {
  const bool isChanging = isActive != g_isActive;
  if (!isChanging) {
    return;
  }
  g_isActive = isActive;
  g_doneModel = nullptr;
  if (isActive) {
    edge::SetRphDoneSlot(reinterpret_cast<void*>(OnDoneButtonClicked));
  } else {
    edge::RestoreRphDoneSlot();
  }
  Log("[handlers] %s", isActive ? "on" : "off");
}

// The two detours are not split hooks: control.h re-enables them after it
// disables the split's.
inline void EnableHooks() {
  MH_EnableHook(
      edge::At(edge155::rva::ContentSettingRPHBubbleModel_CommitChanges));
  MH_EnableHook(
      edge::At(edge155::rva::
                   RegisterProtocolHandlerPermissionRequest_PermissionDecided));
}

// Returns whether this config turns the feature on, the moment control.h lifts
// every profile's blocks (LiftIgnoredHandlers).
inline bool ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  SetActive(wire.protocolHandlersActive != 0);
  return !wasActive && g_isActive;
}

// Before unload: no vtable slot may point into a DLL that is going away.
inline void Remove() {
  SetActive(false);
}

}  // namespace fixedge::protocol_handlers
