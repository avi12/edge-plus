// collapse.h — the split button's "off" direction. One concern: which tab stays
// in front when Edge dissolves a split (the toolbar's split button while split,
// the ⋯ menu's "Separate two tabs"). Every member goes back to the strip as a
// standalone tab, and DoActionToSplitWindow brings the split's first tab to the
// front (kDissolveKeepFirst), or its last when the second pane has focus
// (kDissolveKeepLast); the latter is clamped to the former. This was meant to
// keep the ACTIVE pane, but the action ignores the page it is handed, and
// keeping a third pane or later in front also needs the view re-fed: Edge's
// two-pane teardown then shows the second pane's page. No tab is ever closed
// on this path (docs/re/collapse.md).
#pragma once
#include "edge.h"

namespace fixedge::collapse {

// DoActionToSplitWindow(controller, WebContents*, int action): `action` is an
// edge155::off::SplitAction.
using DoAction_t = void*(__fastcall*)(void*, void*, int);
inline DoAction_t o_do_action = nullptr;

inline void* __fastcall h_do_action(void* controller, void* webContents,
                                    int action) {
  const bool isKeepingLastTab =
      action == static_cast<int>(edge155::off::SplitAction::kDissolveKeepLast);
  if (isKeepingLastTab) {
    Log("[collapse] action 3 -> 2 (the first pane stays in front, the rest "
        "become tabs)");
    action = static_cast<int>(edge155::off::SplitAction::kDissolveKeepFirst);
  }
  return o_do_action(controller, webContents, action);
}

}  // namespace fixedge::collapse
