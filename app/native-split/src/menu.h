// menu.h — the directional-add items on the split's native 3-dot menu. One
// concern: extend SplitTabMenuModel (a ui::SimpleMenuModel) with four commands
// and route them to split_ops::AddPane. The command ids 0x6ac..0x6af sit just
// past Edge's own dense range 0x6a5..0x6ab (docs/re/split-menu.md).
#pragma once
#include <cstring>
#include "edge.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::menu {

using namespace fixedge::edge155;

enum : int { kAddUp = 0x6ac, kAddDown = 0x6ad, kAddLeft = 0x6ae, kAddRight = 0x6af };

// Labels built once into static u16strings (short permanent allocations), so the
// ctor hook can reference them on every menu open without re-allocating.
inline edge::U16String g_label[4];
inline bool g_labels_ready = false;
inline void BuildLabels() {
  const char* text[4] = {"Add pane above", "Add pane below", "Add pane to the left",
                         "Add pane to the right"};
  for (int i = 0; i < 4; ++i) {
    edge::U8View view{text[i], std::strlen(text[i])};
    edge::Utf8ToUtf16(&g_label[i], &view);
  }
  g_labels_ready = true;
}

// SplitTabMenuModel::SplitTabMenuModel — after it builds its items, append ours.
// `self` (rcx) is the model; extra args are passed through untouched.
using Ctor_t = void*(__fastcall*)(void*, void*, void*, void*, void*);
inline Ctor_t o_ctor = nullptr;
inline void* __fastcall h_ctor(void* self, void* a, void* b, void* c, void* d) {
  void* result = o_ctor(self, a, b, c, d);
  if (!g_labels_ready) BuildLabels();
  edge::MenuAddItem(self, kAddUp, &g_label[0]);
  edge::MenuAddItem(self, kAddDown, &g_label[1]);
  edge::MenuAddItem(self, kAddLeft, &g_label[2]);
  edge::MenuAddItem(self, kAddRight, &g_label[3]);
  Log("[menu] appended 4 directional items to model %p", self);
  return result;
}

// SplitTabMenuModel::ExecuteCommand(self=delegate, int command_id, int flags).
using Exec_t = void*(__fastcall*)(void*, int, int);
inline Exec_t o_exec = nullptr;
inline void* __fastcall h_exec(void* self, int command_id, int flags) {
  if (command_id >= kAddUp && command_id <= kAddRight) {
    const Direction dir = command_id == kAddUp     ? Direction::kUp
                          : command_id == kAddDown ? Direction::kDown
                          : command_id == kAddLeft ? Direction::kLeft
                                                   : Direction::kRight;
    split_ops::AddPane(g_active_mcv, dir);
    return nullptr;
  }
  return o_exec(self, command_id, flags);
}

}  // namespace fixedge::menu
