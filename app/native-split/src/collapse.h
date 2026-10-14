// collapse.h — the split button's "off" direction. One concern: when a split is
// collapsed, keep the ACTIVE pane (fill the whole window) and eject the other
// panes back to standalone tabs. Edge already does exactly this natively via
// DoActionToSplitWindow action 2 (dissolve-keeping-tabs + activate active);
// action 3 is the same but keeps the LAST tab. The focused-close path sends 3,
// so we clamp 3 -> 2. No tab is ever closed on this path (docs/re/collapse.md).
#pragma once
#include "edge.h"

namespace fixedge::collapse {

using namespace fixedge::edge155;

using DoAction_t = void*(__fastcall*)(void*, void*, int);
inline DoAction_t o_do_action = nullptr;

inline void* __fastcall h_do_action(void* controller, void* web_contents,
                                    int action) {
  if (action == 3) {
    Log("[collapse] action 3 -> 2 (keep the active pane, eject the rest as "
        "tabs)");
    action = 2;
  }
  return o_do_action(controller, web_contents, action);
}

}  // namespace fixedge::collapse
