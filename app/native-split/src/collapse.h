// collapse.h — the split button's "off" direction. One concern: when a split is
// collapsed, keep the ACTIVE pane (fill the whole window) and eject the other
// panes back to standalone tabs. Edge already does exactly this natively via
// DoActionToSplitWindow's kDissolveKeepActive; kDissolveKeepLast is the same but
// keeps the LAST tab. The focused-close path sends the latter, so we clamp it to
// the former. No tab is ever closed on this path (docs/re/collapse.md).
#pragma once
#include "edge.h"

namespace fixedge::collapse {

using namespace fixedge::edge155;

// DoActionToSplitWindow's `action` (r8d), the 6-way jump table mapped in
// docs/re/collapse.md §5. The names are ours: Edge's own enum was not located.
enum class SplitAction : int {
  kDuplicateLastTab = 0,    // kDuplicateIntoSplit, on the split's last tab
  kDuplicateIntoSplit = 1,  // open the kept tab's URL into the split
  kDissolveKeepActive = 2,  // dissolve, the active pane stays in front
  kDissolveKeepLast = 3,    // dissolve, the split's last tab comes to the front
  kCloseActive = 4,         // CLOSES the active tab — never for collapse
  kReverseSides = 5,        // ReverseTabsInSplit
};

using DoAction_t = void*(__fastcall*)(void*, void*, int);
inline DoAction_t o_do_action = nullptr;

inline void* __fastcall h_do_action(void* controller, void* webContents,
                                    int action) {
  const bool isKeepingLastTab =
      action == static_cast<int>(SplitAction::kDissolveKeepLast);
  if (isKeepingLastTab) {
    Log("[collapse] action 3 -> 2 (keep the active pane, eject the rest as "
        "tabs)");
    action = static_cast<int>(SplitAction::kDissolveKeepActive);
  }
  return o_do_action(controller, webContents, action);
}

}  // namespace fixedge::collapse
