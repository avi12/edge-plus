// menu.h — our items on the split's native ⋯ menu. One concern: extend
// SplitTabMenuModel (a ui::SimpleMenuModel) with the directional adds and the
// "use equal widths / heights" commands, and route each to the pane whose ⋯
// opened the menu. The command ids 0x6ac..0x6b1 sit just past Edge's own dense
// range 0x6a5..0x6ab (docs/re/split-menu.md).
#pragma once
#include <cstddef>
#include <cstring>
#include <iterator>
#include "edge.h"
#include "grip.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::menu {

using namespace fixedge::edge155;

enum class MenuCommand : int {
  kAddUp = 0x6ac,
  kAddDown = 0x6ad,
  kAddLeft = 0x6ae,
  kAddRight = 0x6af,
  kEqualWidths = 0x6b0,
  kEqualHeights = 0x6b1,
};

// What an item does to the pane whose ⋯ opened the menu.
enum class MenuAction : uint8_t {
  kAddPane,
  kEqualize
};

// Each item, in menu order: its command, what it does, the direction it adds a
// pane in or the axis it evens out, and its label.
struct MenuItem {
  MenuCommand command;
  MenuAction action;
  Direction direction;
  Axis axis;
  const char* label;
};
inline constexpr MenuItem kMenuItems[] = {
    {MenuCommand::kAddUp, MenuAction::kAddPane, Direction::kUp, Axis::kVertical,
     "Add pane above"},
    {MenuCommand::kAddDown, MenuAction::kAddPane, Direction::kDown,
     Axis::kVertical, "Add pane below"},
    {MenuCommand::kAddLeft, MenuAction::kAddPane, Direction::kLeft,
     Axis::kHorizontal, "Add pane to the left"},
    {MenuCommand::kAddRight, MenuAction::kAddPane, Direction::kRight,
     Axis::kHorizontal, "Add pane to the right"},
    {MenuCommand::kEqualWidths, MenuAction::kEqualize, Direction::kLeft,
     Axis::kHorizontal, "Use equal widths"},
    {MenuCommand::kEqualHeights, MenuAction::kEqualize, Direction::kUp,
     Axis::kVertical, "Use equal heights"},
};
inline constexpr size_t kMenuItemCount = std::size(kMenuItems);

// The pane whose ⋯ opened the menu (ToggleContextMenu), consumed by the next
// command; without one (a menu opened elsewhere) the active pane is used.
inline void* g_menuMcv = nullptr;
inline int g_menuPane = kNoLeaf;

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

// ControlButtons::ToggleContextMenu: remember whose ⋯ this is, then open it.
using Toggle_t = void(__fastcall*)(void*, void*);
inline Toggle_t o_toggle = nullptr;
inline void __fastcall h_toggle(void* bar, void* argument2) {
  g_menuMcv = nullptr;
  g_menuPane = kNoLeaf;
  void* mcv = nullptr;
  int pane = kNoLeaf;
  if (grip::LocateBar(bar, &mcv, &pane)) {
    g_menuMcv = mcv;
    g_menuPane = pane;
  }
  o_toggle(bar, argument2);
}

// Run one of our commands on the pane whose ⋯ opened the menu (else the
// active pane). False for any other id, which is Edge's own.
inline bool RunCommand(int commandId) {
  for (const MenuItem& item : kMenuItems) {
    const bool isItemCommand = commandId == static_cast<int>(item.command);
    if (!isItemCommand) {
      continue;
    }
    const bool hasMenuPane = g_menuMcv && g_menuPane != kNoLeaf;
    void* mcv = hasMenuPane ? g_menuMcv : g_activeMcv;
    const int pane = hasMenuPane ? g_menuPane : split_ops::ActiveLeaf(mcv);
    g_menuMcv = nullptr;
    g_menuPane = kNoLeaf;
    if (item.action == MenuAction::kAddPane) {
      split_ops::AddPane(mcv, item.direction, pane);
    } else {
      split_ops::EqualizeAround(mcv, pane, item.axis);
    }
    return true;
  }
  return false;
}

// SplitTabMenuModel::ExecuteCommand(self=delegate, int command_id, int
// event_flags): the toolbar's split menu.
using Exec_t = void*(__fastcall*)(void*, int, int);
inline Exec_t o_exec = nullptr;
inline void* __fastcall h_exec(void* self, int commandId, int eventFlags) {
  if (RunCommand(commandId)) {
    return nullptr;
  }
  return o_exec(self, commandId, eventFlags);
}

// SplitWindowControlMenu::BuildMenuModel: the pane bar's ⋯. After Edge's own
// items, a separator and ours.
using Build_t = void(__fastcall*)(void*);
inline Build_t o_build = nullptr;
inline void __fastcall h_build(void* menu) {
  o_build(menu);
  void* model = edge::Field<void*>(menu, off::SplitWindowControlMenu_model);
  if (!model) {
    return;
  }
  if (!g_hasLabels) {
    BuildLabels();
  }
  edge::MenuAddSeparator(model);
  for (size_t i = 0; i < kMenuItemCount; i++) {
    edge::MenuAddItem(model, static_cast<int>(kMenuItems[i].command),
                      &g_labels[i]);
  }
}

// SplitWindowControlMenu::ExecuteCommand(menu, int id, int flags).
using ControlExec_t = void(__fastcall*)(void*, int, int);
inline ControlExec_t o_control_exec = nullptr;
inline void __fastcall h_control_exec(void* menu, int commandId,
                                      int eventFlags) {
  if (RunCommand(commandId)) {
    return;
  }
  o_control_exec(menu, commandId, eventFlags);
}

}  // namespace fixedge::menu
