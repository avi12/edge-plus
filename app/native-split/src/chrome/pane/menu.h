// menu.h — "add a pane" on Edge's split menus: four plain items on the toolbar's
// split menu, and an "Add pane" row with a d-pad of arrows on a pane bar's ⋯
// (whose Edge items are pane_menu.h's).
#pragma once
#include <algorithm>
#include <cstddef>
#include <iterator>
#include "edge/edge.h"
#include "chrome/grip.h"
#include "chrome/icons.h"
#include "split/offer.h"
#include "chrome/pane_menu.h"
#include "split/split_ops.h"
#include "layout/state.h"

namespace edgeplus::menu {

// Just past Edge's own dense range 0x6a5..0x6ab (docs/re/split-menu.md).
enum class MenuCommand : int {
  kAddUp = 0x6ac,
  kAddDown = 0x6ad,
  kAddLeft = 0x6ae,
  kAddRight = 0x6af,
};

// The ⋯ row's id: far from every Edge range, because the menu's
// ShouldExecuteCommandWithoutClosingMenu is shared with other menus
// (offsets.h), and any id it sees that is not ours goes to the original.
inline constexpr int kAddPaneRowCommand = 0x10f1e;

struct MenuItem {
  MenuCommand command = MenuCommand::kAddUp;
  Direction direction = Direction::kUp;
  const char* label = "";
};
inline constexpr MenuItem kMenuItems[] = {
    {MenuCommand::kAddUp, Direction::kUp, "Add pane above"},
    {MenuCommand::kAddDown, Direction::kDown, "Add pane below"},
    {MenuCommand::kAddLeft, Direction::kLeft, "Add pane to the left"},
    {MenuCommand::kAddRight, Direction::kRight, "Add pane to the right"},
};
inline constexpr size_t kMenuItemCount = std::size(kMenuItems);
inline constexpr char kRowLabel[] = "Add pane";
inline constexpr int kNoArrow = -1;

// The pane whose ⋯ opened the menu (ToggleContextMenu), consumed by the next
// action on it; without one (a menu opened elsewhere) an add uses the active
// pane.
inline constinit void* g_menuMcv = nullptr;
inline constinit int g_menuPane = kNoLeaf;
inline void ForgetMenuPane() {
  g_menuMcv = nullptr;
  g_menuPane = kNoLeaf;
}
// The open ⋯ menu's views.
inline constinit void* g_arrows[kMenuItemCount] = {};
inline constinit void* g_addPaneRow = nullptr;
inline constinit void* g_dpad = nullptr;
// The row icon's natural height, before the row was made d-pad tall.
inline constinit int g_addPaneIconHeight = 0;

// Built once and never freed, so every menu open can reference them.
inline constinit edge::U16String g_labels[kMenuItemCount];
inline constinit edge::U16String g_rowLabel;
inline constinit bool g_hasLabels = false;
inline void BuildLabels() {
  for (size_t i = 0; i < kMenuItemCount; i++) {
    edge::Utf8ToUtf16(&g_labels[i], kMenuItems[i].label);
  }
  edge::Utf8ToUtf16(&g_rowLabel, kRowLabel);
  g_hasLabels = true;
}

inline void AddBesideMenuPane(Direction direction) {
  const bool hasMenuPane = g_menuMcv && g_menuPane != kNoLeaf;
  void* mcv = hasMenuPane ? g_menuMcv : g_activeMcv;
  const int pane = hasMenuPane ? g_menuPane : split_ops::ActiveLeaf(mcv);
  ForgetMenuPane();
  offer::AddOfferPane(mcv, direction, pane);
}

// --- the toolbar's split menu (SplitTabMenuModel) ------------------------------

// SplitTabMenuModel::SplitTabMenuModel(model, tsm, menu source, optional tab
// index, one stack argument).
using Ctor_t = void*(__fastcall*)(void*, void*, void*, void*, void*);
inline constinit Ctor_t o_ctor = nullptr;
inline void* __fastcall h_ctor(void* self, void* tsm, void* menuSource,
                               void* optionalTabIndex, void* argument5) {
  void* result = o_ctor(self, tsm, menuSource, optionalTabIndex, argument5);
  if (!g_hasLabels) {
    BuildLabels();
  }
  for (size_t i = 0; i < kMenuItemCount; i++) {
    edge::MenuAddItem(self, static_cast<int>(kMenuItems[i].command),
                      &g_labels[i]);
  }
  return result;
}

// Diagnostic: did the ⋯ split menu even route through WebUISplitTabsControl?
using HandleCtx_t = void*(__fastcall*)(void*, void*, void*, void*);
inline constinit HandleCtx_t o_handle_ctx = nullptr;
inline void* __fastcall h_handle_ctx(void* self, void* argument2,
                                     void* argument3, void* argument4) {
  Log("[diag] WebUISplitTabsControl::HandleContextMenu fired");
  return o_handle_ctx(self, argument2, argument3, argument4);
}

// SplitTabMenuModel::ExecuteCommand(self=delegate, int command_id, int
// event_flags).
using Exec_t = void*(__fastcall*)(void*, int, int);
inline constinit Exec_t o_exec = nullptr;
inline void* __fastcall h_exec(void* self, int commandId, int eventFlags) {
  for (const MenuItem& item : kMenuItems) {
    const bool isItemCommand = commandId == static_cast<int>(item.command);
    if (isItemCommand) {
      AddBesideMenuPane(item.direction);
      return nullptr;
    }
  }
  return o_exec(self, commandId, eventFlags);
}

// --- a pane bar's ⋯ (SplitWindowControlMenu) -----------------------------------

// ControlButtons::ToggleContextMenu.
using Toggle_t = void(__fastcall*)(void*, void*);
inline constinit Toggle_t o_toggle = nullptr;
inline void __fastcall h_toggle(void* bar, void* argument2) {
  ForgetMenuPane();
  std::fill(std::begin(g_arrows), std::end(g_arrows), nullptr);
  g_addPaneRow = nullptr;
  g_dpad = nullptr;
  void* mcv = nullptr;
  int pane = kNoLeaf;
  if (grip::LocateBar(bar, &mcv, &pane)) {
    g_menuMcv = mcv;
    g_menuPane = pane;
  }
  o_toggle(bar, argument2);
}

// SplitWindowControlMenu::BuildMenuModel. It RETURNS the model it built (Run
// hands that to PopulateMenu), so the detour must too.
using Build_t = void*(__fastcall*)(void*);
inline constinit Build_t o_build = nullptr;
inline void* __fastcall h_build(void* menu) {
  void* model = o_build(menu);
  if (!model) {
    return model;
  }
  pane_menu::FitItems(model, g_menuMcv);
  if (!g_hasLabels) {
    BuildLabels();
  }
  edge::MenuAddSeparator(model);
  edge::MenuAddItemWithIcon(model, kAddPaneRowCommand, &g_rowLabel,
                            edge::At(edge155::rva::Icon_SplitScene));
  return model;
}

// A 3 x 3 grid with an arrow in the middle of each edge.
inline constexpr int kDpadCell = 26;
inline constexpr int kDpadSize = 3 * kDpadCell;
// Room on the d-pad's right so the → clears the menu's rounded edge.
inline constexpr int kDpadRightInset = 10;

// (column, row)
inline Point DpadCell(Direction direction) {
  switch (direction) {
    case Direction::kUp:
      return Point{1, 0};
    case Direction::kDown:
      return Point{1, 2};
    case Direction::kLeft:
      return Point{0, 1};
    default:
      return Point{2, 1};
  }
}

// SplitWindowControlMenu::PopulateMenu(menu, root MenuItemView*, model). A menu
// item lays its child views out right-aligned at full height, so the arrows sit
// in one plain container that places them itself.
using Populate_t = void(__fastcall*)(void*, void*, void*);
inline constinit Populate_t o_populate = nullptr;
inline void __fastcall h_populate(void* menu, void* rootItem, void* model) {
  o_populate(menu, rootItem, model);
  void* row =
      rootItem ? edge::MenuItemById(rootItem, kAddPaneRowCommand) : nullptr;
  if (!row) {
    return;
  }
  void* dpad = edge::NewPlainView();
  edge::SetViewPreferredSize(dpad,
                             Size{kDpadSize + kDpadRightInset, kDpadSize});
  for (size_t i = 0; i < kMenuItemCount; i++) {
    const Direction direction = kMenuItems[i].direction;
    void* arrow = edge::NewControlButton(icons::Arrow(direction));
    edge::ViewSetTooltip(arrow, kMenuItems[i].label);
    edge::AddChildViewAt(dpad, arrow, edge::ViewChildCount(dpad));
    const Point cell = DpadCell(direction);
    edge::SetViewBounds(arrow, Rect{cell.x * kDpadCell, cell.y * kDpadCell,
                                    kDpadCell, kDpadCell});
    g_arrows[i] = arrow;
  }
  edge::AddChildViewAt(row, dpad, edge::ViewChildCount(row));
  // A row takes its height from its icon view alone.
  if (void* icon = edge::MenuItemIconView(row)) {
    const Size iconSize = edge::ViewPreferredSize(icon);
    g_addPaneIconHeight = iconSize.height;
    edge::SetViewPreferredSize(icon, Size{iconSize.width, kDpadSize});
    edge::MenuItemRemeasure(row, icon);
  }
  g_addPaneRow = row;
  g_dpad = dpad;
}

// MenuItemView::CalculateProposedLayout: on the d-pad-tall "Add pane" row,
// Edge would centre the icon on a standard row's height and pin the d-pad to
// the top, while the label sits in the middle. Centre both like the label.
using RowLayout_t = void*(__fastcall*)(void*, void*, const void*);
inline constinit RowLayout_t o_row_layout = nullptr;

inline void CentreInRow(void* proposedLayout, const void* child, int height) {
  Rect* bounds = edge::ProposedChildBounds(proposedLayout, child);
  if (!bounds) {
    return;
  }
  bounds->height = height;
  bounds->y = (edge::ProposedHostHeight(proposedLayout) - height) / 2;
}

inline void* __fastcall h_row_layout(void* layoutDelegate, void* proposedLayout,
                                     const void* sizeBounds) {
  void* result = o_row_layout(layoutDelegate, proposedLayout, sizeBounds);
  void* item = edge::MenuItemOfLayoutDelegate(layoutDelegate);
  const bool isAddPaneRow = g_addPaneRow && item == g_addPaneRow;
  if (!isAddPaneRow) {
    return result;
  }
  CentreInRow(proposedLayout, edge::MenuItemIconView(item),
              g_addPaneIconHeight);
  CentreInRow(proposedLayout, g_dpad, kDpadSize);
  return result;
}

inline int ArrowIndexOf(const void* button) {
  for (size_t i = 0; i < kMenuItemCount; i++) {
    const bool isThisArrow = button && g_arrows[i] == button;
    if (isThisArrow) {
      return static_cast<int>(i);
    }
  }
  return kNoArrow;
}

// control.h queues this after the click's own event dispatch, since closing the
// menu destroys the arrow.
inline void RunArrow(Direction direction) {
  edge::CancelActiveMenu();
  Log("[menu] add pane: direction %d", static_cast<int>(direction));
  AddBesideMenuPane(direction);
}

// ShouldExecuteCommandWithoutClosingMenu (shared code, see offsets.h).
using ShouldExecute_t = bool(__fastcall*)(void*, int, const void*);
inline constinit ShouldExecute_t o_should_execute = nullptr;
inline bool __fastcall h_should_execute(void* self, int commandId,
                                        const void* event) {
  const bool isAddPaneRow = commandId == kAddPaneRowCommand;
  return isAddPaneRow || o_should_execute(self, commandId, event);
}

// SplitWindowControlMenu::ExecuteCommand(menu, int id, int flags).
using ControlExec_t = void(__fastcall*)(void*, int, int);
inline constinit ControlExec_t o_control_exec = nullptr;
inline void __fastcall h_control_exec(void* menu, int commandId,
                                      int eventFlags) {
  const bool isAddPaneRow = commandId == kAddPaneRowCommand;
  if (isAddPaneRow) {
    return;
  }
  const bool hasRunOnMenuPane =
      pane_menu::RunItem(commandId, g_menuMcv, g_menuPane);
  if (hasRunOnMenuPane) {
    ForgetMenuPane();
    return;
  }
  o_control_exec(menu, commandId, eventFlags);
}

}  // namespace edgeplus::menu
