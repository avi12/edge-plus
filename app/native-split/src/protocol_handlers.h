// protocol_handlers.h — a site's request to handle a link type
// (navigator.registerProtocolHandler, as Gmail asks for mailto:) is only ever
// blocked when the user says so.
//
// Edge's bubble (no user gesture) and prompt (with one) both block the site for
// good on a non-answer (docs/re/protocol-handlers.md): the bubble commits its
// preselected "Ignore" however it closes, and the prompt's "Just this time"
// lands in the same ignored list as "Block". An ignored site's later requests
// are dropped without a trace. So the bubble acts only on Done, "Just this time"
// remembers nothing, and every block on record is lifted whenever the feature
// turns on (Edge 155 has no settings page that lists them).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "MinHook.h"
#include "config.h"
#include "edge.h"
#include "log.h"

namespace fixedge::protocol_handlers {

// Chromium's PermissionDecision.
enum class Decision : int32_t {
  kAllow = 0,
  kAllowThisTime = 1,  // "Just this time"
  kDeny = 2,           // "Block"
};

inline constinit bool g_isActive = false;

// Done runs the accept callback first and only then closes the bubble
// (CommitChanges), all on the UI thread, so the next CommitChanges for this
// model is Done's.
inline constinit void* g_doneModel = nullptr;

// Installed in the bubble model's vtable slot while the feature is on (the
// slot's own function is an empty `ret`).
inline void __fastcall OnDoneButtonClicked(void* model) {
  g_doneModel = model;
}

// ContentSettingRPHBubbleModel::CommitChanges.
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

// RegisterProtocolHandlerPermissionRequest::PermissionDecided.
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

// Returns how many were lifted. RemoveIgnoredHandler erases from the very list
// it is given, so each handler is first copied out into storage we own. A
// policy block stays (Edge keeps it in the live list) and is not counted.
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

// The two detours stay installed (hooks.h) and pass straight through while the
// feature is off; only the Done slot is written and restored here.
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

// Whether this config turns the feature on: control.h then lifts every
// profile's blocks.
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
