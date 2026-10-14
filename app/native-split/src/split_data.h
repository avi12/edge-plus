// split_data.h — grow the native split's DATA MODEL by one member, the way Edge
// itself does it (RemoveSplitImpl + AddToSplitImpl, docs/re/split-grow-impl.md).
// One concern: make a 2-member split a 3-member split so Edge feeds/controls all
// panes; the view-side container growth + N-tiling (hooks.h) then render them.
// UI thread only. Heavily logged so an autonomous test shows exactly where it
// stops.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "edge.h"

namespace fixedge::split_data {

// Edge's CreateSplit rejects a member vector of size 1, so a split is at least
// two tabs.
inline constexpr int kMinSplitMembers = 2;
// The most members CreateSplitN builds, and the most a split may already have
// for GrowSplit to grow it.
inline constexpr int kMaxCreateMembers = 6;
inline constexpr int kMaxGrowMembers = 7;
// Stack buffers sized past the largest member count either path accepts.
inline constexpr int kMemberBufferSize = 8;
// SplitTabVisualData's ratio as Edge's own default ctor sets it: an even split.
inline constexpr double kEvenSplitRatio = 0.5;
// How far up a faulting stack to look, and how many msedge.dll return
// addresses to log from it.
inline constexpr int kStackScanSlots = 400;
inline constexpr int kMaxLoggedReturnAddresses = 14;

// RemoveSplitImpl's flag as Edge's UpdateTabInSplitImpl passes it: the
// transactional remove, every member back to a standalone tab.
inline constexpr int kTransactionalRemove = 1;

// AddToSplitImpl's last argument, forwarded to NotifySplitTabCreated: what its
// two Edge callers pass (docs/re/split-grow-impl.md).
enum class AddToSplitFlag : int {
  kNewSplit = 0,      // AddToNewSplit
  kRebuiltSplit = 1,  // UpdateTabInSplitImpl, re-adding under the same id
};

inline void LogFault(const char* primitive,
                     const EXCEPTION_POINTERS* exceptionInfo) {
  const auto faultAddress = reinterpret_cast<uintptr_t>(
      exceptionInfo->ExceptionRecord->ExceptionAddress);
  Log("[split] %s faulted code=0x%lx at rva 0x%llx", primitive,
      exceptionInfo->ExceptionRecord->ExceptionCode,
      static_cast<unsigned long long>(faultAddress - edge::g_base));
}

// SEH-guarded UpdateTabInSplit: a bad arg can int3/ud2 and kill the browser. Catch
// it so Edge survives and we can iterate. No C++ objects here, so __try is legal.
inline bool SafeUpdateTabInSplit(void* tsm, void* anchor, int newTabIndex,
                                 int flag) {
  const EXCEPTION_POINTERS* exceptionInfo = nullptr;
  __try {
    edge::UpdateTabInSplit(tsm, anchor, newTabIndex, flag);
    return true;
  } __except (exceptionInfo = GetExceptionInformation(),
              EXCEPTION_EXECUTE_HANDLER) {
    LogFault("UpdateTabInSplit", exceptionInfo);
    return false;
  }
}

inline bool SafeAddToSplit(void* tsm, void* outId, void* splitId,
                           edge::SplitIndexVec* indices, int anchor,
                           void* visual, AddToSplitFlag flag) {
  const EXCEPTION_POINTERS* exceptionInfo = nullptr;
  __try {
    edge::AddToSplitImpl(tsm, outId, splitId, indices, anchor, visual,
                         static_cast<int>(flag));
    return true;
  } __except (exceptionInfo = GetExceptionInformation(),
              EXCEPTION_EXECUTE_HANDLER) {
    LogFault("AddToSplitImpl", exceptionInfo);
    // Scan the faulting stack for return addresses inside msedge.dll's .text so
    // the exact call chain to the crash can be resolved offline.
    const uintptr_t moduleStart = edge::g_base;
    const uintptr_t moduleEnd = edge::g_base + 0x0c000000;
    const auto* stackSlots =
        reinterpret_cast<const uintptr_t*>(exceptionInfo->ContextRecord->Rsp);
    int loggedCount = 0;
    for (int i = 0;
         i < kStackScanSlots && loggedCount < kMaxLoggedReturnAddresses; i++) {
      const uintptr_t slotValue = stackSlots[i];
      const bool isInsideMsedge =
          slotValue >= moduleStart && slotValue < moduleEnd;
      if (isInsideMsedge) {
        Log("[split]   stack[%d] rva 0x%llx", i,
            static_cast<unsigned long long>(slotValue - edge::g_base));
        loggedCount++;
      }
    }
    return false;
  }
}

// optional<SplitTabId>: base::Token (16) at +0, bool has_value at +0x10.
struct OptionalSplitId {
  uint8_t token[16];
  uint8_t hasValue;
  uint8_t padding[7];
};

// SplitTabVisualData is 16 bytes, {int layout @+0, double ratio @+8}; zeroed
// with an even ratio it is what Edge's default ctor builds.
inline void SetEvenRatio(uint8_t* visual) {
  *reinterpret_cast<double*>(visual + 8) = kEvenSplitRatio;
}

// A background tab: neither the active tab nor already in any split.
inline bool IsFreeTab(void* tab, void* active) {
  return tab && tab != active && !edge::TabIsSplit(tab);
}

// The strip index of a free tab, searching from the strip's tail, or -1.
inline int FindFreeTab(void* tsm, void* root, void* active) {
  const int tabCount = edge::TabCount(tsm);
  for (int i = tabCount - 1; i >= 0; i--) {
    void* tab = edge::GetTabAtIndexRecursive(root, i);
    if (IsFreeTab(tab, active)) {
      return i;
    }
  }
  return -1;
}

// Create a fresh 2-pane split from the active tab + one background tab.
// Proven 2-pane native split via AddToNewSplit (its size==1 branch adds active).
inline bool CreateSplit(void* tsm) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool hasActiveTab = tsm && root && active;
  if (!hasActiveTab) {
    Log("[create] tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  if (edge::TabIsSplit(active)) {
    Log("[create] active tab already in a split");
    return false;
  }
  const int freeTabIndex = FindFreeTab(tsm, root, active);
  const bool hasFreeTab = freeTabIndex >= 0;
  if (!hasFreeTab) {
    Log("[create] no free background tab");
    return false;
  }
  edge::SplitIndexVec splitIndices = edge::MakeIndexVec(&freeTabIndex, 1);
  uint8_t visual[16] = {};
  SetEvenRatio(visual);
  uint8_t outId[16] = {};
  edge::AddToNewSplit(tsm, outId, &splitIndices, visual);
  Log("[create] AddToNewSplit with bg tab %d", freeTabIndex);
  return true;
}

// Direct AddToSplitImpl with `requestedPanes` standalone tabs (diagnostic: 2 vs
// N behavior).
inline bool CreateSplitN(void* tsm, int requestedPanes) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool canCreateSplit =
      tsm && root && active && !edge::TabIsSplit(active);
  if (!canCreateSplit) {
    Log("[createN] bad state tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  const int targetPanes =
      std::clamp(requestedPanes, kMinSplitMembers, kMaxCreateMembers);
  const int tabCount = edge::TabCount(tsm);
  int memberIndices[kMemberBufferSize];
  int memberCount = 0;
  const int activeIndex = edge::GetIndexOfTabRecursive(root, active);
  const bool isActiveInStrip = activeIndex >= 0;
  if (isActiveInStrip) {
    memberIndices[memberCount] = activeIndex;
    memberCount++;
  }
  for (int i = 0; i < tabCount && memberCount < targetPanes; i++) {
    void* tab = edge::GetTabAtIndexRecursive(root, i);
    if (IsFreeTab(tab, active)) {
      memberIndices[memberCount] = i;
      memberCount++;
    }
  }
  const bool hasEnoughMembers = memberCount >= kMinSplitMembers;
  if (!hasEnoughMembers) {
    Log("[createN] not enough free tabs (%d)", memberCount);
    return false;
  }
  std::sort(memberIndices, memberIndices + memberCount);
  const int anchor = memberIndices[0];
  uint8_t splitId[16] = {};
  edge::CreateRandomToken(splitId);
  uint8_t visual[16] = {};
  SetEvenRatio(visual);
  uint8_t outId[16] = {};
  edge::SplitIndexVec splitIndices =
      edge::MakeIndexVec(memberIndices, static_cast<size_t>(memberCount));
  Log("[createN] AddToSplitImpl n=%d anchor=%d idx=[%d,%d,%d]", memberCount,
      anchor, memberIndices[0], memberCount > 1 ? memberIndices[1] : -1,
      memberCount > 2 ? memberIndices[2] : -1);
  const bool isAdded =
      SafeAddToSplit(tsm, outId, splitId, &splitIndices, anchor, visual,
                     AddToSplitFlag::kNewSplit);
  Log("[createN] AddToSplitImpl %s (%d panes)", isAdded ? "done" : "FAILED",
      memberCount);
  // Even if the call faulted, did the DATA model get the split? (isolates a
  // data-model fault from a synchronous view-refresh fault.)
  void* activeAfter = edge::ActiveTab(tsm);
  OptionalSplitId splitAfter = {};
  if (activeAfter) {
    edge::TabGetSplit(activeAfter, &splitAfter);
  }
  if (splitAfter.hasValue) {
    uint64_t memberVector[3] = {};  // {data, size, capacity}
    edge::GetTabsAndIndicesInSplit(tsm, memberVector, splitAfter.token);
    Log("[createN] post: data model split has %d members",
        static_cast<int>(memberVector[1]));
  } else {
    Log("[createN] post: active tab not in a split");
  }
  return isAdded;
}

// Add one pane to the active split (Arc-style incremental grow). If the active
// tab isn't split yet, create a 2-pane split instead. Edge's split is identified
// by one base::Token; to add a member we dissolve it and re-create it under a
// fresh id with every old member plus one free background tab, preserving the
// orientation/ratio. (UpdateTabInSplit only *replaces* a member, never appends.)
inline bool GrowSplit(void* tsm) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool hasActiveTab = tsm && root && active;
  if (!hasActiveTab) {
    Log("[grow] tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  OptionalSplitId split = {};
  edge::TabGetSplit(active, &split);
  if (!split.hasValue) {
    Log("[grow] active tab not split yet - creating a 2-pane split");
    return CreateSplitN(tsm, kMinSplitMembers);
  }

  // Capture the live members (TabModel*) and the visual before dissolving. The
  // vector is {data@0, size@8, cap@0x10}; each entry is {TabModel*, int}, 16B.
  uint64_t memberVector[3] = {};
  edge::GetTabsAndIndicesInSplit(tsm, memberVector, split.token);
  auto* memberEntries = reinterpret_cast<uint8_t*>(memberVector[0]);
  const int memberCount = memberEntries ? static_cast<int>(memberVector[1]) : 0;
  const bool isGrowableCount =
      memberCount >= kMinSplitMembers && memberCount <= kMaxGrowMembers;
  if (!isGrowableCount) {
    Log("[grow] unexpected member count %d", memberCount);
    return false;
  }
  void* memberTabs[kMemberBufferSize];
  for (int i = 0; i < memberCount; i++) {
    memberTabs[i] = *reinterpret_cast<void**>(
        memberEntries + static_cast<std::ptrdiff_t>(i) * 16);
  }
  uint8_t visual[16] = {};
  if (void* splitData = edge::GetSplitData(tsm, split.token)) {
    // SplitTabData+8 is the live visual: keep its orientation and ratio.
    std::memcpy(visual, reinterpret_cast<uint8_t*>(splitData) + 8,
                sizeof(visual));
  } else {
    SetEvenRatio(visual);
  }

  // A free background tab to become the new pane.
  const int freeTabIndex = FindFreeTab(tsm, root, active);
  const bool hasFreeTab = freeTabIndex >= 0;
  if (!hasFreeTab) {
    Log("[grow] no free background tab to add");
    return false;
  }
  void* newTab = edge::GetTabAtIndexRecursive(root, freeTabIndex);
  if (!newTab) {
    Log("[grow] new tab resolve failed at %d", freeTabIndex);
    return false;
  }

  // Dissolve, then re-resolve every tab's (now standalone) index and re-create.
  edge::RemoveSplitImpl(tsm, split.token, kTransactionalRemove);
  int memberIndices[kMemberBufferSize + 1];  // every old member + the new tab
  int indexCount = 0;
  for (int i = 0; i < memberCount; i++) {
    const int memberIndex = edge::GetIndexOfTabRecursive(root, memberTabs[i]);
    const bool isMemberFound = memberIndex >= 0;
    if (!isMemberFound) {
      Log("[grow] lost member %d after remove", i);
      return false;
    }
    memberIndices[indexCount] = memberIndex;
    indexCount++;
  }
  const int newTabIndex = edge::GetIndexOfTabRecursive(root, newTab);
  const bool isNewTabFound = newTabIndex >= 0;
  if (!isNewTabFound) {
    Log("[grow] lost new tab after remove");
    return false;
  }
  memberIndices[indexCount] = newTabIndex;
  indexCount++;
  std::sort(memberIndices, memberIndices + indexCount);
  const int anchor = memberIndices[0];
  uint8_t splitId[16] = {};
  edge::CreateRandomToken(splitId);
  uint8_t outId[16] = {};
  edge::SplitIndexVec splitIndices =
      edge::MakeIndexVec(memberIndices, static_cast<size_t>(indexCount));
  Log("[grow] AddToSplitImpl n=%d anchor=%d (was %d members)", indexCount,
      anchor, memberCount);
  const bool isAdded =
      SafeAddToSplit(tsm, outId, splitId, &splitIndices, anchor, visual,
                     AddToSplitFlag::kNewSplit);
  Log("[grow] AddToSplitImpl %s (now %d panes)", isAdded ? "done" : "FAILED",
      indexCount);
  return isAdded;
}

// Collapse the active split: dissolve it so every member becomes a standalone
// tab again and the active tab fills the whole window. Nothing is closed. Works
// for any member count (the view shrinks back to one pane on the notify).
inline bool CollapseSplit(void* tsm) {
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  if (!active) {
    Log("[collapse] no active tab");
    return false;
  }
  OptionalSplitId split = {};
  edge::TabGetSplit(active, &split);
  if (!split.hasValue) {
    Log("[collapse] active tab is not in a split");
    return false;
  }
  edge::RemoveSplitImpl(tsm, split.token, kTransactionalRemove);
  Log("[collapse] dissolved split; active tab fills the window");
  return true;
}

}  // namespace fixedge::split_data
