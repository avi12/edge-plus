// split_data.h — grow the native split's DATA MODEL by one member, the way Edge
// itself does it (RemoveSplitImpl + AddToSplitImpl, docs/re/split-grow-impl.md),
// so Edge feeds and controls every pane; hooks.h renders them. UI thread only.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include "edge.h"

namespace edgeplus::split_data {

// Edge's CreateSplit rejects a member vector of size 1, so a split is at least
// two tabs.
inline constexpr int kMinSplitMembers = 2;
// The most members CreateSplitN builds, and the most a split may already have
// for GrowSplit to grow it.
inline constexpr int kMaxCreateMembers = 6;
inline constexpr int kMaxGrowMembers = 7;
inline constexpr int kMemberBufferSize = 8;
// SplitTabVisualData's ratio as Edge's own default ctor sets it.
inline constexpr double kEvenSplitRatio = 0.5;
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

// A bad argument can int3/ud2 and kill the browser. No C++ objects here, so
// __try is legal.
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
    // Log the msedge.dll return addresses on the faulting stack, so the call
    // chain can be resolved offline.
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

// SplitTabLayout, the visual's layout and the split view's orientation
// (MCV_orientation): 0 lays the members side by side, any other value one
// above the other.
enum class SplitLayout : int32_t {
  kColumns = 0,
  kRows = 1,
};

inline void SetLayout(uint8_t* visual, SplitLayout layout) {
  *reinterpret_cast<int32_t*>(visual) = static_cast<int32_t>(layout);
}

// What a split keeps when it is re-created under a new id: its visual
// (orientation and ratio, handed to AddToSplitImpl) and Edge's per-split
// "open left links to the right" toggle, which a NEW split would take from a
// pref instead.
struct SplitSettings {
  uint8_t visual[16] = {};
  bool isLinkMode = false;
};

inline SplitSettings ReadSettings(void* tsm, void* splitToken) {
  SplitSettings settings;
  void* splitData = edge::GetSplitData(tsm, splitToken);
  if (!splitData) {
    SetEvenRatio(settings.visual);
    return settings;
  }
  // SplitTabData+8 is the live visual: keep its orientation and ratio.
  std::memcpy(settings.visual, reinterpret_cast<uint8_t*>(splitData) + 8,
              sizeof(settings.visual));
  settings.isLinkMode =
      edge::Field<bool>(splitData, edge155::off::SPLITDATA_link_mode);
  return settings;
}

inline void RestoreLinkMode(void* tsm, void* newSplitToken,
                            const SplitSettings& settings) {
  if (void* splitData = edge::GetSplitData(tsm, newSplitToken)) {
    edge::Field<bool>(splitData, edge155::off::SPLITDATA_link_mode) =
        settings.isLinkMode;
  }
}

inline bool IsLinkModeOn(void* tsm, void* splitTab) {
  OptionalSplitId split = {};
  edge::TabGetSplit(splitTab, &split);
  void* splitData =
      split.hasValue ? edge::GetSplitData(tsm, split.token) : nullptr;
  return splitData &&
         edge::Field<bool>(splitData, edge155::off::SPLITDATA_link_mode);
}

inline int MemberCountOf(void* tsm, void* tab) {
  OptionalSplitId split = {};
  edge::TabGetSplit(tab, &split);
  return split.hasValue ? edge::SplitMembers(tsm, split.token).Count() : 0;
}

inline bool IsFreeTab(void* tab, void* active) {
  return tab && tab != active && !edge::TabIsSplit(tab);
}

// Searches from the strip's tail; -1 when none.
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

// The active tab + one background tab: AddToNewSplit's size==1 branch adds the
// active tab itself.
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
  edge::SplitIndexVec splitIndices =
      edge::MakeIndexVec(std::span(&freeTabIndex, 1));
  uint8_t visual[16] = {};
  SetEvenRatio(visual);
  uint8_t outId[16] = {};
  edge::AddToNewSplit(tsm, outId, &splitIndices, visual);
  Log("[create] AddToNewSplit with bg tab %d", freeTabIndex);
  return true;
}

// Diagnostic: AddToSplitImpl directly with `requestedPanes` standalone tabs.
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
  edge::SplitIndexVec splitIndices = edge::MakeIndexVec(
      std::span(memberIndices).first(static_cast<size_t>(memberCount)));
  Log("[createN] AddToSplitImpl n=%d anchor=%d idx=[%d,%d,%d]", memberCount,
      anchor, memberIndices[0], memberCount > 1 ? memberIndices[1] : -1,
      memberCount > 2 ? memberIndices[2] : -1);
  const bool isAdded =
      SafeAddToSplit(tsm, outId, splitId, &splitIndices, anchor, visual,
                     AddToSplitFlag::kNewSplit);
  Log("[createN] AddToSplitImpl %s (%d panes)", isAdded ? "done" : "FAILED",
      memberCount);
  // Even after a fault, log whether the data model got the split: tells a
  // data-model fault from a synchronous view-refresh fault.
  void* activeAfter = edge::ActiveTab(tsm);
  OptionalSplitId splitAfter = {};
  if (activeAfter) {
    edge::TabGetSplit(activeAfter, &splitAfter);
  }
  if (splitAfter.hasValue) {
    Log("[createN] post: data model split has %d members",
        edge::SplitMembers(tsm, splitAfter.token).Count());
  } else {
    Log("[createN] post: active tab not in a split");
  }
  return isAdded;
}

// Edge orders a split's members, and so the panes it feeds, by strip index, and
// a grown split's new pane is its last: so the new member must come last.
inline void PlaceAfterMembers(void* tsm, void* root,
                              std::span<void* const> memberTabs, void* newTab) {
  int lastMemberIndex = -1;
  for (void* memberTab : memberTabs) {
    lastMemberIndex = std::max(lastMemberIndex,
                               edge::GetIndexOfTabRecursive(root, memberTab));
  }
  const int newTabIndex = edge::GetIndexOfTabRecursive(root, newTab);
  const bool isFound = lastMemberIndex >= 0 && newTabIndex >= 0;
  if (!isFound) {
    return;
  }
  // Moving a tab from before the split shifts the members down by one.
  const int afterMembers =
      newTabIndex < lastMemberIndex ? lastMemberIndex : lastMemberIndex + 1;
  const bool isPlaced = newTabIndex == afterMembers;
  if (isPlaced) {
    return;
  }
  Log("[grow] moving the new tab %d -> %d (after the split)", newTabIndex,
      afterMembers);
  edge::MoveTabTo(tsm, newTabIndex, afterMembers);
}

// The unsplit active tab and `newTab` (null: a free background tab) as a new
// split laid out `layout`. Edge orders a split's members, and so its panes, by
// strip index: `isNewTabFirst` puts the new tab before the active one, on the
// left or on top.
inline bool CreateSplitWith(void* tsm, void* newTab, SplitLayout layout,
                            bool isNewTabFirst) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool canCreateSplit = root && active && !edge::TabIsSplit(active);
  if (!canCreateSplit) {
    Log("[create] tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  if (!newTab) {
    newTab = edge::TabModelAt(tsm, FindFreeTab(tsm, root, active));
  }
  if (!IsFreeTab(newTab, active)) {
    Log("[create] the new tab %p is not a free background tab", newTab);
    return false;
  }
  const int newTabIndex = edge::GetIndexOfTabRecursive(root, newTab);
  const int activeIndex = edge::GetIndexOfTabRecursive(root, active);
  // Moving the new tab to the active tab's index puts it on the other side.
  const bool isOnWrongSide = (newTabIndex < activeIndex) != isNewTabFirst;
  if (isOnWrongSide) {
    edge::MoveTabTo(tsm, newTabIndex, activeIndex);
  }
  int memberIndices[kMinSplitMembers] = {
      edge::GetIndexOfTabRecursive(root, active),
      edge::GetIndexOfTabRecursive(root, newTab)};
  const bool areBothFound = memberIndices[0] >= 0 && memberIndices[1] >= 0;
  if (!areBothFound) {
    Log("[create] lost a tab after placing the new one");
    return false;
  }
  std::sort(std::begin(memberIndices), std::end(memberIndices));
  uint8_t splitId[16] = {};
  edge::CreateRandomToken(splitId);
  uint8_t visual[16] = {};
  SetEvenRatio(visual);
  SetLayout(visual, layout);
  uint8_t outId[16] = {};
  edge::SplitIndexVec splitIndices =
      edge::MakeIndexVec(std::span(memberIndices));
  const bool isAdded =
      SafeAddToSplit(tsm, outId, splitId, &splitIndices, memberIndices[0],
                     visual, AddToSplitFlag::kNewSplit);
  Log("[create] AddToSplitImpl [%d,%d] layout %d, new tab %s: %s",
      memberIndices[0], memberIndices[1], static_cast<int>(layout),
      isNewTabFirst ? "first" : "second", isAdded ? "done" : "FAILED");
  return isAdded;
}

// Add `newTab` (a TabModel*; null: a free background tab) to the active split,
// or make a 2-pane split if the active tab is not in one. Edge has no append
// (UpdateTabInSplit only replaces a member), so the split is dissolved and
// re-created under a fresh id with every old member plus the new tab, keeping
// its orientation and ratio.
inline bool GrowSplit(void* tsm, void* newTab = nullptr) {
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
    if (newTab) {
      return CreateSplitWith(tsm, newTab, SplitLayout::kColumns, false);
    }
    return CreateSplitN(tsm, kMinSplitMembers);
  }

  const edge::SplitMembers members(tsm, split.token);
  const int memberCount = members.Count();
  const bool isGrowableCount =
      memberCount >= kMinSplitMembers && memberCount <= kMaxGrowMembers;
  if (!isGrowableCount) {
    Log("[grow] unexpected member count %d", memberCount);
    return false;
  }
  void* memberTabs[kMemberBufferSize];
  for (int i = 0; i < memberCount; i++) {
    memberTabs[i] = members.TabAt(i);
  }
  const SplitSettings settings = ReadSettings(tsm, split.token);

  if (!newTab) {
    const int freeTabIndex = FindFreeTab(tsm, root, active);
    const bool hasFreeTab = freeTabIndex >= 0;
    if (!hasFreeTab) {
      Log("[grow] no free background tab to add");
      return false;
    }
    newTab = edge::GetTabAtIndexRecursive(root, freeTabIndex);
  }
  const bool isNewTabFree = IsFreeTab(newTab, active);
  if (!isNewTabFree) {
    Log("[grow] the new tab %p is not a free background tab", newTab);
    return false;
  }
  PlaceAfterMembers(
      tsm, root, std::span(memberTabs).first(static_cast<size_t>(memberCount)),
      newTab);

  // Dissolving moves tabs: every index is re-resolved after it.
  edge::RemoveSplitImpl(tsm, split.token, kTransactionalRemove);
  int memberIndices[kMemberBufferSize + 1];
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
  edge::SplitIndexVec splitIndices = edge::MakeIndexVec(
      std::span(memberIndices).first(static_cast<size_t>(indexCount)));
  Log("[grow] AddToSplitImpl n=%d anchor=%d (was %d members)", indexCount,
      anchor, memberCount);
  uint8_t visual[16] = {};
  std::memcpy(visual, settings.visual, sizeof(visual));
  // Past two panes the DLL lays them out; Edge faults building a split of
  // more than two members one above the other (AddToSplitImpl).
  SetLayout(visual, SplitLayout::kColumns);
  const bool isAdded =
      SafeAddToSplit(tsm, outId, splitId, &splitIndices, anchor, visual,
                     AddToSplitFlag::kNewSplit);
  if (isAdded) {
    RestoreLinkMode(tsm, outId, settings);
  }
  Log("[grow] AddToSplitImpl %s (now %d panes)", isAdded ? "done" : "FAILED",
      indexCount);
  return isAdded;
}

inline int MemberShowing(void* tsm, void* root,
                         std::span<void* const> memberTabs,
                         const void* contents) {
  const int memberCount = static_cast<int>(memberTabs.size());
  for (int i = 0; i < memberCount; i++) {
    const int tabIndex = edge::GetIndexOfTabRecursive(root, memberTabs[i]);
    const bool isShowingContents =
        tabIndex >= 0 && edge::TabWebContentsAt(tsm, tabIndex) == contents;
    if (isShowingContents) {
      return i;
    }
  }
  return -1;
}

// The active split's pages in member order, the order the feeder gives the
// panes (member i in pane i). Returns how many were written; 0 when unsplit.
inline int ActiveSplitContents(void* tsm,
                               std::span<void*, kMemberBufferSize> out) {
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  if (!active) {
    return 0;
  }
  OptionalSplitId split = {};
  edge::TabGetSplit(active, &split);
  if (!split.hasValue) {
    return 0;
  }
  const edge::SplitMembers members(tsm, split.token);
  const int count = std::min(members.Count(), kMemberBufferSize);
  for (int i = 0; i < count; i++) {
    out[i] = edge::TabContents(members.TabAt(i));
  }
  return count;
}

// What happens to the tab of the member that leaves a split.
enum class MemberFate : uint8_t {
  kClose,
  kKeepAsTab,
};

// Take the member showing `closingContents` out of the active split. Edge
// removes a member only by dissolving the whole split, so the rest are re-split
// under a fresh id with the same visual. Returns how many members remain (1: the
// split is gone), or 0 if nothing was done.
inline int CloseMember(void* tsm, const void* closingContents,
                       const void* activeContents,
                       MemberFate fate = MemberFate::kClose) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool hasActiveTab = tsm && root && active;
  if (!hasActiveTab) {
    return 0;
  }
  OptionalSplitId split = {};
  edge::TabGetSplit(active, &split);
  if (!split.hasValue) {
    return 0;
  }
  const edge::SplitMembers members(tsm, split.token);
  const int memberCount = members.Count();
  const bool isKnownSize =
      memberCount >= kMinSplitMembers && memberCount <= kMemberBufferSize;
  if (!isKnownSize) {
    Log("[close] unexpected member count %d", memberCount);
    return 0;
  }
  void* memberTabs[kMemberBufferSize];
  for (int i = 0; i < memberCount; i++) {
    memberTabs[i] = members.TabAt(i);
  }
  const std::span<void* const> liveMemberTabs =
      std::span(memberTabs).first(static_cast<size_t>(memberCount));
  const int closing = MemberShowing(tsm, root, liveMemberTabs, closingContents);
  const int activating =
      MemberShowing(tsm, root, liveMemberTabs, activeContents);
  const bool hasBothMembers =
      closing >= 0 && activating >= 0 && closing != activating;
  if (!hasBothMembers) {
    Log("[close] member not found (closing=%d activating=%d)", closing,
        activating);
    return 0;
  }
  const SplitSettings settings = ReadSettings(tsm, split.token);

  edge::RemoveSplitImpl(tsm, split.token, kTransactionalRemove);
  if (fate == MemberFate::kClose) {
    edge::CloseTabAt(tsm,
                     edge::GetIndexOfTabRecursive(root, memberTabs[closing]));
  }

  const int remaining = memberCount - 1;
  if (remaining >= kMinSplitMembers) {
    int memberIndices[kMemberBufferSize];
    int indexCount = 0;
    for (int i = 0; i < memberCount; i++) {
      if (i == closing) {
        continue;
      }
      const int memberIndex = edge::GetIndexOfTabRecursive(root, memberTabs[i]);
      if (memberIndex < 0) {
        Log("[close] lost member %d after the close", i);
        return 0;
      }
      memberIndices[indexCount] = memberIndex;
      indexCount++;
    }
    std::sort(memberIndices, memberIndices + indexCount);
    uint8_t splitId[16] = {};
    edge::CreateRandomToken(splitId);
    uint8_t outId[16] = {};
    edge::SplitIndexVec splitIndices = edge::MakeIndexVec(
        std::span(memberIndices).first(static_cast<size_t>(indexCount)));
    uint8_t visual[16] = {};
    std::memcpy(visual, settings.visual, sizeof(visual));
    const bool isResplit =
        SafeAddToSplit(tsm, outId, splitId, &splitIndices, memberIndices[0],
                       visual, AddToSplitFlag::kNewSplit);
    if (!isResplit) {
      Log("[close] re-split of %d members FAILED", indexCount);
      return 0;
    }
    RestoreLinkMode(tsm, outId, settings);
  }
  edge::ActivateTabAt(
      tsm, edge::GetIndexOfTabRecursive(root, memberTabs[activating]));
  Log("[close] closed member %d of %d; %d remain", closing, memberCount,
      remaining);
  return remaining;
}

// Every member becomes a standalone tab again; nothing is closed.
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

// By value, so splits can be collected before any is dissolved: dissolving one
// shifts tab indices, but never another's id.
struct SplitToken {
  uint8_t bytes[16];
};

inline bool IsSameToken(const SplitToken& token, const uint8_t* other) {
  return std::memcmp(token.bytes, other, sizeof(token.bytes)) == 0;
}

// Dissolve every split of more than two panes into standalone tabs, leaving
// Edge's own two-pane splits alone, so no grown split outlives the DLL and
// renders broken under stock Edge. Returns how many were dissolved.
inline int BreakUpGrownSplits(void* tsm) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  if (!root) {
    return 0;
  }
  // Collect first, dissolve after: RemoveSplitImpl moves tabs.
  std::vector<SplitToken> grownSplits;
  const int tabCount = edge::TabCount(tsm);
  for (int i = 0; i < tabCount; i++) {
    void* tab = edge::GetTabAtIndexRecursive(root, i);
    OptionalSplitId split = {};
    edge::TabGetSplit(tab, &split);
    if (!split.hasValue) {
      continue;
    }
    const bool isSeen = std::any_of(grownSplits.begin(), grownSplits.end(),
                                    [&](const SplitToken& seen) {
                                      return IsSameToken(seen, split.token);
                                    });
    if (isSeen) {
      continue;
    }
    const bool isGrownSplit =
        edge::SplitMembers(tsm, split.token).Count() > kMinSplitMembers;
    if (isGrownSplit) {
      SplitToken token = {};
      std::memcpy(token.bytes, split.token, sizeof(token.bytes));
      grownSplits.push_back(token);
    }
  }
  for (SplitToken& token : grownSplits) {
    edge::RemoveSplitImpl(tsm, token.bytes, kTransactionalRemove);
  }
  if (!grownSplits.empty()) {
    Log("[breakup] dissolved %zu grown split(s) into tabs", grownSplits.size());
  }
  return static_cast<int>(grownSplits.size());
}

}  // namespace edgeplus::split_data
