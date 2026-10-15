// menu.h — "add a pane" on Edge's split menus. One concern: offer the four
// directional adds and run them beside the pane they were asked for.
//   - The toolbar's split menu (SplitTabMenuModel): four plain items, ids
//     0x6ac..0x6af, just past Edge's own dense range 0x6a5..0x6ab
//     (docs/re/split-menu.md).
//   - A pane bar's ⋯ (SplitWindowControlMenu): ONE "Add pane" row holding a
//     d-pad of ↑ ↓ ← → buttons. The row itself does nothing; a click on an
//     arrow closes the menu and adds a pane beside that pane in that direction.
#pragma once
#include <algorithm>
#include <cstddef>
#include <iterator>
#include "edge.h"
#include "grip.h"
#include "icons.h"
#include "offer.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::menu {

using namespace fixedge::edge155;

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

// Each directional add, in menu (and arrow) order: its toolbar command, the
// direction and its label.
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
inline constexpr char kRowLabel[] = "Add pane";
inline constexpr int kNoArrow = -1;

// The pane whose ⋯ opened the menu (ToggleContextMenu), consumed by the next
// add; without one (a menu opened elsewhere) the active pane is used.
inline void* g_menuMcv = nullptr;
inline int g_menuPane = kNoLeaf;
// The open ⋯ menu's arrow buttons.
inline void* g_arrows[kMenuItemCount] = {};
// The open ⋯ menu's "Add pane" row (MenuItemView) and its d-pad, centred in
// the row by h_row_layout; and the row icon's natural height (before the row
// was made d-pad tall).
inline void* g_addPaneRow = nullptr;
inline void* g_dpad = nullptr;
inline int g_addPaneIconHeight = 0;

// Labels built once into static u16strings (short permanent allocations), so
// the hooks can reference them on every menu open without re-allocating.
inline edge::U16String g_labels[kMenuItemCount];
inline edge::U16String g_rowLabel;
inline bool g_hasLabels = false;
inline void BuildLabels() {
  for (size_t i = 0; i < kMenuItemCount; i++) {
    edge::Utf8ToUtf16(&g_labels[i], kMenuItems[i].label);
  }
  edge::Utf8ToUtf16(&g_rowLabel, kRowLabel);
  g_hasLabels = true;
}

// Add a pane in `direction` beside the menu's pane (else the active pane),
// offering the window's other tabs for it.
inline void AddBesideMenuPane(Direction direction) {
  const bool hasMenuPane = g_menuMcv && g_menuPane != kNoLeaf;
  void* mcv = hasMenuPane ? g_menuMcv : g_activeMcv;
  const int pane = hasMenuPane ? g_menuPane : split_ops::ActiveLeaf(mcv);
  g_menuMcv = nullptr;
  g_menuPane = kNoLeaf;
  offer::AddOfferPane(mcv, direction, pane);
}

// --- the toolbar's split menu (SplitTabMenuModel) ------------------------------

// SplitTabMenuModel::SplitTabMenuModel — after it builds its items, append
// ours. `self` (rcx) is the model; the rest (the TabStripModel, the menu source
// and the optional tab index, then one stack argument) pass through untouched.
using Ctor_t = void*(__fastcall*)(void*, void*, void*, void*, void*);
inline Ctor_t o_ctor = nullptr;
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
inline HandleCtx_t o_handle_ctx = nullptr;
inline void* __fastcall h_handle_ctx(void* self, void* argument2,
                                     void* argument3, void* argument4) {
  Log("[diag] WebUISplitTabsControl::HandleContextMenu fired");
  return o_handle_ctx(self, argument2, argument3, argument4);
}

// SplitTabMenuModel::ExecuteCommand(self=delegate, int command_id, int
// event_flags). Our ids add a pane; every other id is Edge's own.
using Exec_t = void*(__fastcall*)(void*, int, int);
inline Exec_t o_exec = nullptr;
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

// ControlButtons::ToggleContextMenu: remember whose ⋯ this is, then open it.
using Toggle_t = void(__fastcall*)(void*, void*);
inline Toggle_t o_toggle = nullptr;
inline void __fastcall h_toggle(void* bar, void* argument2) {
  g_menuMcv = nullptr;
  g_menuPane = kNoLeaf;
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
// hands that to PopulateMenu), so the detour must too. After Edge's own items:
// a separator and the "Add pane" row.
using Build_t = void*(__fastcall*)(void*);
inline Build_t o_build = nullptr;
inline void* __fastcall h_build(void* menu) {
  void* model = o_build(menu);
  if (!model) {
    return model;
  }
  if (!g_hasLabels) {
    BuildLabels();
  }
  edge::MenuAddSeparator(model);
  edge::MenuAddItemWithIcon(model, kAddPaneRowCommand, &g_rowLabel,
                            edge::At(rva::Icon_SplitScene));
  return model;
}

// The d-pad: a 3 x 3 grid of kDpadCell DIP cells, an arrow in the middle of
// each edge (↑ top, ← → middle row, ↓ bottom), the centre empty.
inline constexpr int kDpadCell = 26;
inline constexpr int kDpadSize = 3 * kDpadCell;
// Room on the d-pad's right so the → clears the menu's rounded edge.
inline constexpr int kDpadRightInset = 10;

// The arrow's cell (column, row) in the d-pad grid.
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

// SplitWindowControlMenu::PopulateMenu(menu, root MenuItemView*, model): once
// Edge has made the item views, put the d-pad on the "Add pane" row. A menu
// item lays its child views out right-aligned at full height, so the four
// arrows sit in one plain container that places them itself.
using Populate_t = void(__fastcall*)(void*, void*, void*);
inline Populate_t o_populate = nullptr;
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
  // A row takes its height from its icon view alone: make that view as tall
  // as the d-pad and have the row measure again (h_row_layout then centres
  // the icon and the d-pad in it).
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
inline RowLayout_t o_row_layout = nullptr;

// Give `child` `height` in the proposed layout, centred in its row.
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

// The index of `button` among the open ⋯ menu's arrows, else kNoArrow.
inline int ArrowIndexOf(const void* button) {
  for (size_t i = 0; i < kMenuItemCount; i++) {
    const bool isThisArrow = button && g_arrows[i] == button;
    if (isThisArrow) {
      return static_cast<int>(i);
    }
  }
  return kNoArrow;
}

// An arrow was clicked (control.h runs this from the UI thread's queue, after
// the click's own event dispatch, since closing the menu destroys the arrow):
// close the menu, then add the pane.
inline void RunArrow(Direction direction) {
  edge::CancelActiveMenu();
  Log("[menu] add pane: direction %d", static_cast<int>(direction));
  AddBesideMenuPane(direction);
}

// ShouldExecuteCommandWithoutClosingMenu (shared code, see offsets.h): a click
// on the row itself (not on an arrow, whose button takes its own clicks) keeps
// the menu open. Every other id: the original.
using ShouldExecute_t = bool(__fastcall*)(void*, int, const void*);
inline ShouldExecute_t o_should_execute = nullptr;
inline bool __fastcall h_should_execute(void* self, int commandId,
                                        const void* event) {
  const bool isAddPaneRow = commandId == kAddPaneRowCommand;
  return isAddPaneRow || o_should_execute(self, commandId, event);
}

// SplitWindowControlMenu::ExecuteCommand(menu, int id, int flags): Edge's
// items run as before; the "Add pane" row itself does nothing.
using ControlExec_t = void(__fastcall*)(void*, int, int);
inline ControlExec_t o_control_exec = nullptr;
inline void __fastcall h_control_exec(void* menu, int commandId,
                                      int eventFlags) {
  const bool isEdgeItem = commandId != kAddPaneRowCommand;
  if (isEdgeItem) {
    o_control_exec(menu, commandId, eventFlags);
  }
}

}  // namespace fixedge::menu
