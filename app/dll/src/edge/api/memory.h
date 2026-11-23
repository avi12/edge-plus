// memory.h — Edge's own heap: its operator new and delete, so a buffer we
// hand Edge can be reallocated or freed by it, and the BackupRefPtr reference
// a raw_ptr takes.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// What constructing a raw_ptr from `pointee` does: takes a BackupRefPtr
// reference when the pointee lives in PartitionAlloc's BRP pool.
inline void AcquireRawPtrRef(void* pointee) {
  const auto* poolBaseGlobal =
      static_cast<const uint64_t*>(At(edge155::rva::BRP_pool_base_global));
  if (!poolBaseGlobal) {
    return;
  }
  const uint64_t poolBase = *poolBaseGlobal;
  const bool isInPool = (reinterpret_cast<uint64_t>(pointee) &
                         edge155::rva::BRP_pool_mask) == poolBase;
  if (!isInPool) {
    return;
  }
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::RawPtrBackupRef_AcquireInternal)(pointee);
}

// Edge's non-View operator new (so a buffer we hand to a split primitive can be
// reallocated/freed by Edge's own allocator).
inline void* EdgeNew(size_t size) {
  using Fn = void*(__fastcall*)(size_t);
  return Callable<Fn>(edge155::rva::EdgeOperatorNew)(size);
}
inline void EdgeDelete(void* memory) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::EdgeOperatorDelete)(memory);
}

}  // namespace edgeplus::edge
