// collapse.h — which tab stays in front when Edge dissolves a split: always the
// first (kDissolveKeepLast is clamped to kDissolveKeepFirst). Keeping the active
// pane was the goal, but the action ignores the page it is handed, and keeping a
// third pane or later also needs the view re-fed: Edge's two-pane teardown then
// shows the second pane's page (docs/re/collapse.md).
#pragma once
#include "edge.h"

namespace edgeplus::collapse {

// DoActionToSplitWindow(controller, WebContents*, SplitAction).
using DoAction_t = void*(__fastcall*)(void*, void*, int);
inline constinit DoAction_t o_do_action = nullptr;

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

}  // namespace edgeplus::collapse
