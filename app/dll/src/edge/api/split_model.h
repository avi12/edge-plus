// split_model.h — the native split's data model in the TabStripModel: which
// tabs share a split, and the primitives Edge adds, updates and removes one
// with. A split id is a 16-byte base::Token; every `splitId` / `outSplitId`
// below points at one.
#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include "edge/api/binding.h"
#include "edge/api/memory.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline void CreateRandomToken(void* outToken) {
  using Fn = void*(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::TokenCreateRandom)(outToken);
}

// The split primitives' index argument: {int* data; size_t size; size_t cap}
// (NOT libc++ vector) — confirmed from the caller storing rodata {1,1} at +8.
struct SplitIndexVec {
  int32_t* data = nullptr;
  uint64_t size = 0;
  uint64_t capacity = 0;
};
static_assert(sizeof(SplitIndexVec) == 24,
              "the split primitives read 24 bytes");

inline SplitIndexVec MakeIndexVec(std::span<const int32_t> indices) {
  const size_t count = indices.size();
  auto* buffer = static_cast<int32_t*>(EdgeNew(count * sizeof(int32_t)));
  for (size_t i = 0; i < count; i++) {
    buffer[i] = indices[i];
  }
  return SplitIndexVec{buffer, count, count};
}

inline bool TabIsSplit(void* tab) {
  using Fn = bool(__fastcall*)(void*);
  return reinterpret_cast<Fn>(
      VirtualFunction(tab, edge155::off::TABMODEL_vtbl_IsSplit))(tab);
}
// Fills `outOptionalSplitId`, a 24-byte optional<SplitTabId>: the Token @0, its
// has-value bool @0x10.
inline void TabGetSplit(void* tab, void* outOptionalSplitId) {
  using Fn = void(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(VirtualFunction(
      tab, edge155::off::TABMODEL_vtbl_GetSplit))(tab, outOptionalSplitId);
}
struct OptionalSplitId {
  uint8_t token[16];
  uint8_t hasValue;
  uint8_t padding[7];
};
inline OptionalSplitId TabSplitId(void* tab) {
  OptionalSplitId splitId = {};
  TabGetSplit(tab, &splitId);
  return splitId;
}
inline bool IsSameSplit(void* tab, void* otherTab) {
  const OptionalSplitId splitId = TabSplitId(tab);
  const OptionalSplitId otherSplitId = TabSplitId(otherTab);
  const bool isBothInSplits =
      (splitId.hasValue != 0u) && (otherSplitId.hasValue != 0u);
  return isBothInSplits && std::memcmp(splitId.token, otherSplitId.token,
                                       sizeof(splitId.token)) == 0;
}

inline void* GetSplitData(void* tsm, void* splitId) {
  using Fn = void*(__fastcall*)(void*, void*);
  return Callable<Fn>(edge155::rva::GetSplitData)(tsm, splitId);
}
// Fills `outVector`, a 24-byte {data, size, capacity} vector.
inline void GetTabsAndIndicesInSplit(void* tsm, void* outVector,
                                     void* splitId) {
  using Fn = void*(__fastcall*)(void*, void*, void*);
  Callable<Fn>(edge155::rva::GetTabsAndIndicesInSplit)(tsm, outVector, splitId);
}

inline void RemoveSplitImpl(void* tsm, void* splitId, int flag) {
  using Fn = void*(__fastcall*)(void*, void*, int);
  Callable<Fn>(edge155::rva::TabStripModel_RemoveSplitImpl)(tsm, splitId, flag);
}
// 7 args: 4 register + 3 stack ([rsp+0x20/0x28/0x30]).
inline void AddToSplitImpl(void* tsm, void* outSplitId, void* splitId,
                           SplitIndexVec* indices, int anchor, void* visualData,
                           int flag) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*, int, void*, int);
  Callable<Fn>(edge155::rva::TabStripModel_AddToSplitImpl)(
      tsm, outSplitId, splitId, indices, anchor, visualData, flag);
}
// AddToNewSplit(tsm, out base::Token*, SplitIndexVec*, SplitTabVisualData*) — creates
// a 2-pane split (its size==1 branch adds the active tab).
inline void AddToNewSplit(void* tsm, void* outSplitId, SplitIndexVec* indices,
                          void* visualData) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*);
  Callable<Fn>(edge155::rva::TabStripModel_AddToNewSplit)(tsm, outSplitId,
                                                          indices, visualData);
}
// UpdateTabInSplit(tsm, anchor=last split member TabModel*, int newTabIndex, int flag)
// — Edge's own "add a tab to the current split": runs the whole remove+move+add
// transaction internally (how OpenTabInCurrentSplitWindow adds a pane).
inline void UpdateTabInSplit(void* tsm, void* anchorMember, int newTabIndex,
                             int flag) {
  using Fn = void*(__fastcall*)(void*, void*, int, int);
  Callable<Fn>(edge155::rva::TabStripModel_UpdateTabInSplit)(tsm, anchorMember,
                                                             newTabIndex, flag);
}

// A split's members as Edge lists them (GetTabsAndIndicesInSplit): a vector
// {data, size, capacity} of {TabModel*, int} entries that Edge allocates for
// the caller, freed with Edge's allocator when this goes out of scope.
struct SplitMembers {
  SplitMembers(void* tsm, void* splitId) {
    GetTabsAndIndicesInSplit(tsm, &list, splitId);
  }
  ~SplitMembers() {
    if (list.data) {
      EdgeDelete(list.data);
    }
  }
  SplitMembers(const SplitMembers&) = delete;
  SplitMembers& operator=(const SplitMembers&) = delete;
  SplitMembers(SplitMembers&&) = delete;
  SplitMembers& operator=(SplitMembers&&) = delete;

  [[nodiscard]] int Count() const {
    return list.data ? static_cast<int>(list.size) : 0;
  }
  // Member `i`'s tab model (0 <= i < Count()).
  [[nodiscard]] void* TabAt(int i) const {
    return Field<void*>(
        list.data + static_cast<size_t>(i) * edge155::off::SPLITMEMBER_size, 0);
  }

  struct {
    uint8_t* data = nullptr;
    uint64_t size = 0;
    uint64_t capacity = 0;
  } list;
};

}  // namespace edgeplus::edge
