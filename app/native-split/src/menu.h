// menu.h — the directional-add items on the split's native 3-dot menu. One
// concern: extend SplitTabMenuModel (a ui::SimpleMenuModel) with four commands
// and route them to split_ops::AddPane. The command ids 0x6ac..0x6af sit just
// past Edge's own dense range 0x6a5..0x6ab (docs/re/split-menu.md).
#pragma once
#include <cstddef>
#include <cstring>
#include <iterator>
#include "edge.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::menu {

using namespace fixedge::edge155;

enum class MenuCommand : int {
  kAddUp = 0x6ac,
  kAddDown = 0x6ad,
  kAddLeft = 0x6ae,
  kAddRight = 0x6af
};

// Each directional item, in menu order: its command, the direction it adds a
// pane in, and its label.
struct MenuItem {
  MenuCommand command;
  Direction direction;
  const char* label;
};
inline constexpr MenuItem kMenuItems[] = {
    {MenuCommand::kAddUp, Direction::kUp, "Add pane above"},
    {MenuCommand::kAddDown, Direction::kDown, "Add pane below"},
    {MenuCommand::kAddLeft, Direction::kLeft, "Add pane to the left"},
    {MenuCommand::kAddRight, Direction::kRight, "Add pane to the right"},
};
inline constexpr size_t kMenuItemCount = std::size(kMenuItems);

// Labels built once into static u16strings (short permanent allocations), so the
// ctor hook can reference them on every menu open without re-allocating.
inline edge::U16String g_labels[kMenuItemCount];
inline bool g_hasLabels = false;
inline void BuildLabels() {
  for (size_t i = 0; i < kMenuItemCount; i++) {
    const char* label = kMenuItems[i].label;
    const edge::U8View view{label, std::strlen(label)};
    edge::Utf8ToUtf16(&g_labels[i], &view);
  }
  g_hasLabels = true;
}

// SplitTabMenuModel::SplitTabMenuModel — after it builds its items, append ours.
// `self` (rcx) is the model; the rest (the TabStripModel, the menu source and
// the optional tab index, then one stack argument) pass through untouched.
using Ctor_t = void*(__fastcall*)(void*, void*, void*, void*, void*);
inline Ctor_t o_ctor = nullptr;
inline void* __fastcall h_ctor(void* self, void* tsm, void* menuSource,
                               void* optionalTabIndex, void* argument5) {
  void* result = o_ctor(self, tsm, menuSource, optionalTabIndex, argument5);
  Log("[menu] SplitTabMenuModel ctor fired self=%p", self);
  if (!g_hasLabels) {
    BuildLabels();
  }
  for (size_t i = 0; i < kMenuItemCount; i++) {
    edge::MenuAddItem(self, static_cast<int>(kMenuItems[i].command),
                      &g_labels[i]);
  }
  Log("[menu] appended %zu directional items to model %p", kMenuItemCount,
      self);
  return result;
}

// Diagnostic: did the ⋯ split menu even route through WebUISplitTabsControl?
using HandleCtx_t = void*(__fastcall*)(void*, void*, void*, void*);
inline HandleCtx_t o_handle_ctx = nullptr;
inline void* __fastcall h_handle_ctx(void* self, void* argument2,
                                     void* argument3, void* argument4) {
  Log("[diag] WebUISplitTabsControl::HandleContextMenu fired");
  return o_handle_ctx(self, argument2, argument3, argument4);
}

// SplitTabMenuModel::ExecuteCommand(self=delegate, int command_id, int
// event_flags). Our commands add a pane; every other id is Edge's own.
using Exec_t = void*(__fastcall*)(void*, int, int);
inline Exec_t o_exec = nullptr;
inline void* __fastcall h_exec(void* self, int commandId, int eventFlags) {
  for (const MenuItem& item : kMenuItems) {
    const bool isItemCommand = commandId == static_cast<int>(item.command);
    if (isItemCommand) {
      split_ops::AddPane(g_activeMcv, item.direction);
      return nullptr;
    }
  }
  return o_exec(self, commandId, eventFlags);
}

}  // namespace fixedge::menu
