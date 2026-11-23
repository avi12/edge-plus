// protocol_registry.h — web protocol handlers: the registry's ignored
// handlers, a permission request's decision, and the Done button slot of the
// handler bubble.
#pragma once
#include <windows.h>
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// The handler bubble model's OnDoneButtonClicked vtable slot; null when the
// vtable has no place in this build.
inline void** RphDoneSlotAddress() {
  void* vtable = At(edge155::rva::ContentSettingRPHBubbleModel_vftable);
  if (!vtable) {
    return nullptr;
  }
  return reinterpret_cast<void**>(
      static_cast<uint8_t*>(vtable) +
      edge155::off::BubbleModel_OnDoneButtonClicked_slot);
}

inline void* RphDoneSlot() {
  void* const* slot = RphDoneSlotAddress();
  return slot ? *slot : nullptr;
}

// Whether the Done slot still holds Edge's own empty virtual (the build check
// for the slot, before we ever write it).
inline bool RphDoneSlotIsStock() {
  const void* const emptyVirtual =
      At(edge155::rva::ContentSettingBubbleModel_EmptyVirtual);
  return (emptyVirtual != nullptr) && RphDoneSlot() == emptyVirtual;
}

// Point the handler bubble's Done slot at `function` (the vtable lives in
// read-only data, so the page is made writable for the one pointer).
inline void SetRphDoneSlot(void* function) {
  void** address = RphDoneSlotAddress();
  if (!address) {
    return;
  }
  DWORD oldProtection = 0;
  if (VirtualProtect(address, sizeof(void*), PAGE_READWRITE, &oldProtection)) {
    *reinterpret_cast<void**>(address) = function;
    VirtualProtect(address, sizeof(void*), oldProtection, &oldProtection);
  }
}

inline void RestoreRphDoneSlot() {
  SetRphDoneSlot(At(edge155::rva::ContentSettingBubbleModel_EmptyVirtual));
}

// A permission prompt's decision, as PermissionDecided reads it.
struct PermissionDecision {
  // False when the prompt was dismissed without a choice.
  bool hasValue = false;
  // 0 = Allow; anything else Edge treats as Block.
  int32_t value = 0;
};

inline PermissionDecision ReadPermissionDecision(const void* decision) {
  return {Field<uint8_t>(decision,
                         edge155::off::PERMISSION_DECISION_has_value) == 1,
          Field<int32_t>(decision, edge155::off::PERMISSION_DECISION_value)};
}

// Room for one custom_handlers::ProtocolHandler that our DLL owns: the bytes
// are ours, the strings and GURL inside are built and freed by Edge's own copy
// constructor and destructor.
struct alignas(16) ProtocolHandlerStorage {
  uint8_t bytes[edge155::off::PROTOCOL_HANDLER_size] = {};
};

// The profile's ProtocolHandlerRegistry (created if it does not exist yet).
inline void* ProtocolHandlerRegistryFor(void* profile) {
  using Fn = void*(__fastcall*)(void*);
  return Callable<Fn>(
      edge155::rva::ProtocolHandlerRegistryFactory_GetForBrowserContext)(
      profile);
}

inline size_t IgnoredHandlerCount(void* registry) {
  return Field<uint64_t>(registry, edge155::off::RPH_REGISTRY_ignored_size);
}

// Copy-construct the registry's ignored handler `index` into `storage`, so it
// stays valid while the list it came from is erased from. Pair every copy with
// DestroyProtocolHandler.
inline void CopyIgnoredHandler(void* registry, size_t index,
                               ProtocolHandlerStorage* storage) {
  auto* data =
      Field<uint8_t*>(registry, edge155::off::RPH_REGISTRY_ignored_data);
  using Fn = void*(__fastcall*)(void*, const void*);
  Callable<Fn>(edge155::rva::ProtocolHandler_CopyConstructor)(
      storage, data + index * edge155::off::PROTOCOL_HANDLER_size);
}

inline void DestroyProtocolHandler(ProtocolHandlerStorage* storage) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::ProtocolHandler_Destructor)(storage);
}

// Lift the block on `handler` (a copy, never an element of the ignored list)
// and save the profile's prefs, as the bubble's Allow does before it accepts.
inline void RemoveIgnoredHandler(void* registry,
                                 const ProtocolHandlerStorage* handler) {
  constexpr bool kShouldSave = true;
  using Fn = void(__fastcall*)(void*, const void*, bool);
  Callable<Fn>(edge155::rva::ProtocolHandlerRegistry_RemoveIgnoredHandler)(
      registry, handler, kShouldSave);
}

}  // namespace edgeplus::edge
