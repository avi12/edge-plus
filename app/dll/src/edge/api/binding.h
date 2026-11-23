// binding.h — the way into msedge.dll, under every other edge/api header:
// its load address, an offsets.h entry as an address, a typed callable or a
// hook switch, a patched code byte, a field of an Edge object, its vtable and
// whether memory is readable. offsets.h holds the raw numbers. On x64 the
// `__fastcall` tags are documentation, not behaviour.
#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include "MinHook.h"
#include "core/log.h"
#include "edge/translation.h"

namespace edgeplus::edge {

// msedge.dll's load address, set once by Bind().
inline constinit uintptr_t g_base = 0;

inline bool Bind(HMODULE module) {
  g_base = reinterpret_cast<uintptr_t>(module);
  return g_base != 0;
}

// Where offsets.h's `rva` is in the running msedge.dll (translation.h): itself on
// the pinned build, else wherever its signature found it. kUnresolved when it was
// not found or its feature stood down.
inline uintptr_t Rva(uintptr_t pinnedRva) {
  return translation::Translate(pinnedRva);
}

// The address of an entry of offsets.h; null when it has no place in this build.
inline void* At(uintptr_t pinnedRva) {
  const uintptr_t rva = Rva(pinnedRva);
  const bool isResolved = rva != translation::kUnresolved;
  return isResolved ? reinterpret_cast<void*>(g_base + rva) : nullptr;
}

// A function of offsets.h as a typed pointer: where the running build has it,
// or, for an entry it could not place, a stub that does nothing and returns a
// zero value (null, false, 0). A feature with an unplaced entry stands down
// (resolve.h), so the stub runs only when code shared between features reaches
// an entry of one that stood down; it is logged once, never a crash.
template <class Function>
struct UnresolvedStub;

template <class Result, class... Arguments>
struct UnresolvedStub<Result(__fastcall*)(Arguments...)> {
  static Result __fastcall Call(Arguments...) {
    static std::atomic<bool> isLogged{false};
    const bool wasLogged = isLogged.exchange(true);
    if (!wasLogged) {
      Log("[skip] a call to an Edge function this build has no place for");
    }
    if constexpr (!std::is_void_v<Result>) {
      return Result{};
    }
  }
};

template <class Function>
inline Function Callable(uintptr_t pinnedRva) {
  void* address = At(pinnedRva);
  if (!address) {
    return &UnresolvedStub<Function>::Call;
  }
  return reinterpret_cast<Function>(address);
}

// MH_EnableHook / MH_DisableHook for one entry of offsets.h. Never pass them
// At()'s answer directly: null is MinHook's MH_ALL_HOOKS, so an entry this
// build could not place would switch every hook instead of none.
inline MH_STATUS EnableHookAt(uintptr_t pinnedRva) {
  void* target = At(pinnedRva);
  return target ? MH_EnableHook(target) : MH_ERROR_NOT_CREATED;
}

inline MH_STATUS DisableHookAt(uintptr_t pinnedRva) {
  void* target = At(pinnedRva);
  return target ? MH_DisableHook(target) : MH_ERROR_NOT_CREATED;
}

// Overwrite one code byte — used to lift the index<2 guard. Returns the byte
// that was there (for the log).
inline uint8_t PatchByte(uintptr_t rva, uint8_t value) {
  void* address = At(rva);
  if (!address) {
    return 0;
  }
  DWORD oldProtection = 0;
  const uint8_t previousByte = *reinterpret_cast<uint8_t*>(address);
  if (VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtection)) {
    *reinterpret_cast<uint8_t*>(address) = value;
    VirtualProtect(address, 1, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), address, 1);
  }
  return previousByte;
}

inline constexpr uint8_t kNopOpcode = 0x90;
// SetWebContentsAtIndex's `cmp index, 2` becomes `cmp index, 0xff`.
inline constexpr uint8_t kLiftedIndexCap = 0xff;

// Overwrite `byteCount` code bytes with NOPs, copying what was there into
// `savedBytes` (for the log) — used to neutralize a 2-pane-only CHECK so a grown
// split survives.
inline void PatchNop(uintptr_t rva, size_t byteCount, uint8_t* savedBytes) {
  void* address = At(rva);
  if (!address) {
    return;
  }
  DWORD oldProtection = 0;
  auto* code = reinterpret_cast<uint8_t*>(address);
  if (VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE,
                     &oldProtection)) {
    for (size_t i = 0; i < byteCount; i++) {
      savedBytes[i] = code[i];
      code[i] = kNopOpcode;
    }
    VirtualProtect(address, byteCount, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), address, byteCount);
  }
}

// Typed lvalue at object+offset. The one place raw pointer arithmetic on Edge
// objects lives; callers read/write fields by name via the Mcv*/Container*
// helpers of the other edge/api headers.
template <class T>
inline T& Field(void* object, uintptr_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(object) + offset);
}
// The same field of an object we may only read.
template <class T>
inline const T& Field(const void* object, uintptr_t offset) {
  return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(object) +
                                     offset);
}

// The function in `object`'s vtable at `slotOffset`. offsets.h records a vtable
// slot as the byte offset the disassembly indexes it by, not as a slot number.
inline void* VirtualFunction(void* object, uintptr_t slotOffset) {
  void* const* vtable = *reinterpret_cast<void* const**>(object);
  return vtable[slotOffset / sizeof(void*)];
}

// Where `object`'s vtable sits in msedge.dll, to name its class from the PDB.
inline uintptr_t VtableRva(void* object) {
  return reinterpret_cast<uintptr_t>(Field<void*>(object, 0)) - g_base;
}

inline bool IsReadable(const void* address, size_t size) {
  MEMORY_BASIC_INFORMATION info{};
  const bool isQueried = VirtualQuery(address, &info, sizeof(info)) != 0;
  const uint8_t* end = reinterpret_cast<const uint8_t*>(address) + size;
  const uint8_t* regionEnd =
      reinterpret_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
  const bool isMapped = info.State == MEM_COMMIT &&
                        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
  return isQueried && isMapped && end <= regionEnd;
}

}  // namespace edgeplus::edge
