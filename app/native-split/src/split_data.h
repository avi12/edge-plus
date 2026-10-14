// split_data.h — grow the native split's DATA MODEL by one member, the way Edge
// itself does it (RemoveSplitImpl + AddToSplitImpl, docs/re/split-grow-impl.md).
// One concern: make a 2-member split a 3-member split so Edge feeds/controls all
// panes; the view-side container growth + N-tiling (hooks.h) then render them.
// UI thread only. Heavily logged so an autonomous test shows exactly where it
// stops.
#pragma once
#include <cstdint>
#include <cstring>
#include "edge.h"

namespace fixedge::split_data {

// SEH-guarded UpdateTabInSplit: a bad arg can int3/ud2 and kill the browser. Catch
// it so Edge survives and we can iterate. No C++ objects here, so __try is legal.
inline bool SafeUpdateTabInSplit(void* tsm, void* anchor, int new_index,
                                 int flag) {
  EXCEPTION_POINTERS* ep = nullptr;
  __try {
    edge::UpdateTabInSplit(tsm, anchor, new_index, flag);
    return true;
  } __except (ep = GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER) {
    const auto addr =
        reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
    Log("[split] UpdateTabInSplit faulted code=0x%lx at rva 0x%llx",
        ep->ExceptionRecord->ExceptionCode,
        static_cast<unsigned long long>(addr - edge::g_base));
    return false;
  }
}

inline bool SafeAddToSplit(void* tsm, void* out_id, void* id,
                           edge::SplitIndexVec* vec, int anchor, void* visual,
                           int flag) {
  EXCEPTION_POINTERS* ep = nullptr;
  __try {
    edge::AddToSplitImpl(tsm, out_id, id, vec, anchor, visual, flag);
    return true;
  } __except (ep = GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER) {
    const auto addr =
        reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
    Log("[split] AddToSplitImpl faulted code=0x%lx at rva 0x%llx",
        ep->ExceptionRecord->ExceptionCode,
        static_cast<unsigned long long>(addr - edge::g_base));
    // Scan the faulting stack for return addresses inside msedge.dll's .text so
    // the exact call chain to the crash can be resolved offline.
    const uintptr_t lo = edge::g_base, hi = edge::g_base + 0x0c000000;
    auto* sp = reinterpret_cast<uintptr_t*>(ep->ContextRecord->Rsp);
    int found = 0;
    for (int i = 0; i < 400 && found < 14; ++i) {
      const uintptr_t v = sp[i];
      if (v >= lo && v < hi) {
        Log("[split]   stack[%d] rva 0x%llx", i,
            static_cast<unsigned long long>(v - edge::g_base));
        ++found;
      }
    }
    return false;
  }
}

// optional<SplitTabId>: base::Token (16) at +0, bool present at +0x10.
struct OptSplitId {
  uint8_t token[16];
  uint8_t present;
  uint8_t pad[7];
};

// Find a background tab (not the active tab, not already in any split), returning
// its current strip index, or -1.
inline int FindFreeTab(void* tsm, void* root, void* active) {
  const int tab_count = edge::TabCount(tsm);
  for (int i = tab_count - 1; i >= 0; --i) {
    void* t = edge::GetTabAtIndexRecursive(root, i);
    if (t && t != active && !edge::TabIsSplit(t)) {
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
  if (!tsm || !root || !active) {
    Log("[create] tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  if (edge::TabIsSplit(active)) {
    Log("[create] active tab already in a split");
    return false;
  }
  const int bg = FindFreeTab(tsm, root, active);
  if (bg < 0) {
    Log("[create] no free background tab");
    return false;
  }
  int one = bg;
  edge::SplitIndexVec indices = edge::MakeIndexVec(&one, 1);
  uint8_t visual[16] = {};
  *reinterpret_cast<double*>(visual + 8) = 0.5;  // {layout=0, ratio=0.5}
  uint8_t out_id[16] = {};
  edge::AddToNewSplit(tsm, out_id, &indices, visual);
  Log("[create] AddToNewSplit with bg tab %d", bg);
  return true;
}

// Direct AddToSplitImpl with `want` standalone tabs (diagnostic: 2 vs N behavior).
inline bool CreateSplitN(void* tsm, int want) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  if (!tsm || !root || !active || edge::TabIsSplit(active)) {
    Log("[createN] bad state tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  if (want < 2) {
    want = 2;
  }
  if (want > 6) {
    want = 6;
  }
  const int tc = edge::TabCount(tsm);
  int idx[8];
  int n = 0;
  const int active_idx = edge::GetIndexOfTabRecursive(root, active);
  if (active_idx >= 0) {
    idx[n++] = active_idx;
  }
  for (int i = 0; i < tc && n < want; ++i) {
    void* t = edge::GetTabAtIndexRecursive(root, i);
    if (t && t != active && !edge::TabIsSplit(t)) {
      idx[n++] = i;
    }
  }
  if (n < 2) {
    Log("[createN] not enough free tabs (%d)", n);
    return false;
  }
  for (int a = 0; a < n; ++a) {
    for (int b = a + 1; b < n; ++b) {
      if (idx[b] < idx[a]) {
        int t = idx[a];
        idx[a] = idx[b];
        idx[b] = t;
      }
    }
  }
  const int anchor = idx[0];
  uint8_t id[16] = {};
  edge::CreateRandomToken(id);
  uint8_t visual[16] = {};
  *reinterpret_cast<double*>(visual + 8) = 0.5;
  uint8_t out_id[16] = {};
  edge::SplitIndexVec indices = edge::MakeIndexVec(idx, static_cast<size_t>(n));
  Log("[createN] AddToSplitImpl n=%d anchor=%d idx=[%d,%d,%d]", n, anchor,
      idx[0], n > 1 ? idx[1] : -1, n > 2 ? idx[2] : -1);
  const bool ok = SafeAddToSplit(tsm, out_id, id, &indices, anchor, visual, 0);
  Log("[createN] AddToSplitImpl %s (%d panes)", ok ? "done" : "FAILED", n);
  // Even if the call faulted, did the DATA model get the split? (isolates a
  // data-model fault from a synchronous view-refresh fault.)
  void* a2 = edge::ActiveTab(tsm);
  OptSplitId o2 = {};
  if (a2) {
    edge::TabGetSplit(a2, &o2);
  }
  if (o2.present) {
    uint64_t v2[3] = {};
    edge::GetTabsAndIndicesInSplit(tsm, v2, o2.token);
    Log("[createN] post: data model split has %d members",
        static_cast<int>(v2[1]));
  } else {
    Log("[createN] post: active tab not in a split");
  }
  return ok;
}

// Add one pane to the active split (Arc-style incremental grow). If the active
// tab isn't split yet, create a 2-pane split instead. Edge's split is identified
// by one base::Token; to add a member we dissolve it and re-create it under a
// fresh id with every old member plus one free background tab, preserving the
// orientation/ratio. (UpdateTabInSplit only *replaces* a member, never appends.)
inline bool GrowSplit(void* tsm) {
  void* root = tsm ? edge::RootCollection(tsm) : nullptr;
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  if (!tsm || !root || !active) {
    Log("[grow] tsm=%p root=%p active=%p", tsm, root, active);
    return false;
  }
  OptSplitId opt = {};
  edge::TabGetSplit(active, &opt);
  if (!opt.present) {
    Log("[grow] active tab not split yet - creating a 2-pane split");
    return CreateSplitN(tsm, 2);
  }

  // Capture the live members (TabModel*) and the visual before dissolving.
  uint64_t members_vec[3] =
      {};  // {data@0, size@8, cap@0x10}; entry {TabModel*,int} 16B
  edge::GetTabsAndIndicesInSplit(tsm, members_vec, opt.token);
  auto* begin = reinterpret_cast<uint8_t*>(members_vec[0]);
  const int mc = begin ? static_cast<int>(members_vec[1]) : 0;
  if (mc < 2 || mc > 7) {
    Log("[grow] unexpected member count %d", mc);
    return false;
  }
  void* member_tabs[8];
  for (int i = 0; i < mc; ++i) {
    member_tabs[i] = *reinterpret_cast<void**>(begin + i * 16);
  }
  uint8_t visual[16] = {};
  if (void* sd = edge::GetSplitData(tsm, opt.token)) {
    std::memcpy(visual, reinterpret_cast<uint8_t*>(sd) + 8,
                sizeof(visual));  // keep orientation+ratio
  } else {
    *reinterpret_cast<double*>(visual + 8) = 0.5;
  }

  // A free background tab to become the new pane.
  const int bg = FindFreeTab(tsm, root, active);
  if (bg < 0) {
    Log("[grow] no free background tab to add");
    return false;
  }
  void* new_tab = edge::GetTabAtIndexRecursive(root, bg);
  if (!new_tab) {
    Log("[grow] new tab resolve failed at %d", bg);
    return false;
  }

  // Dissolve, then re-resolve every tab's (now standalone) index and re-create.
  edge::RemoveSplitImpl(tsm, opt.token, 1);
  int idx[9];
  int n = 0;
  for (int i = 0; i < mc; ++i) {
    const int ix = edge::GetIndexOfTabRecursive(root, member_tabs[i]);
    if (ix < 0) {
      Log("[grow] lost member %d after remove", i);
      return false;
    }
    idx[n++] = ix;
  }
  const int nix = edge::GetIndexOfTabRecursive(root, new_tab);
  if (nix < 0) {
    Log("[grow] lost new tab after remove");
    return false;
  }
  idx[n++] = nix;
  for (int a = 0; a < n; ++a) {
    for (int b = a + 1; b < n; ++b) {
      if (idx[b] < idx[a]) {
        int t = idx[a];
        idx[a] = idx[b];
        idx[b] = t;
      }
    }
  }
  const int anchor = idx[0];
  uint8_t id[16] = {};
  edge::CreateRandomToken(id);
  uint8_t out_id[16] = {};
  edge::SplitIndexVec indices = edge::MakeIndexVec(idx, static_cast<size_t>(n));
  Log("[grow] AddToSplitImpl n=%d anchor=%d (was %d members)", n, anchor, mc);
  const bool ok = SafeAddToSplit(tsm, out_id, id, &indices, anchor, visual, 0);
  Log("[grow] AddToSplitImpl %s (now %d panes)", ok ? "done" : "FAILED", n);
  return ok;
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
  OptSplitId opt = {};
  edge::TabGetSplit(active, &opt);
  if (!opt.present) {
    Log("[collapse] active tab is not in a split");
    return false;
  }
  edge::RemoveSplitImpl(tsm, opt.token, 1);
  Log("[collapse] dissolved split; active tab fills the window");
  return true;
}

}  // namespace fixedge::split_data
