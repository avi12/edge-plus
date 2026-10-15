# The pane bar's ⋯ menu — Edge's items and the "Add pane" d-pad row

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**; their one home
in code is `app/native-split/src/offsets.h`.

The ⋯ button in a pane's hover bar opens `SplitWindowControlMenu`, not the
toolbar's `SplitTabMenuModel` (`split-menu.md`). We append one row to it:
Edge's split icon, the label "Add pane", and a d-pad of ↑ ← → ↓ buttons. The
row itself does nothing; an arrow adds a pane beside the pane whose ⋯ it was,
in that direction. The new pane opens on Edge's tab offer, the starting page
its own split button opens (`split-offer.md`). Edge's own items are fitted to
the N-pane split first (code: `src/pane_menu.h`).

## Edge's own items

`BuildMenuModel` (`0xa017fbc`) adds them in this order: 2, 3, 5, then 0 or 1
(the switch to the other orientation), 6 (with the link-mode feature), 4.
`ExecuteCommand` (`0xa0186c0`) switches on the id through a jump table at
`0x12603f00`. In code: `off::ControlMenuCommand`.

| id | item | Edge runs | here |
|---|---|---|---|
| 0 / 1 | Switch to vertical / horizontal | `SetVerticalSplitStateForTab(active, bool)` (`0xa020960`) → `TabStripModel::UpdateSplitLayout` | left out of a split of three or more: its layout is the tree, which ignores the orientation |
| 2 | Open this webpage in a new tab | `DoActionToSplitWindow` 1 or 0: a copy of the split's **first** or **last** tab's page (`collapse.md` §5) | a copy of the ⋯'s own pane |
| 3 | Separate two tabs | `DoActionToSplitWindow` 2 or 3 (dissolve, first or last tab in front) | left out of a split of three or more: the toolbar's split button does it |
| 4 | Send feedback | `chrome::OpenEdgeFeedbackDialog` | left out of every split |
| 5 | Switch left and right tabs | `DoActionToSplitWindow` 5 → `ReverseTabsInSplit`: the first member moves one step on | left out of a split of three or more: it swaps only the first two panes; dragging a pane onto another swaps any two |
| 6 | Open left links to the right (toggle) | `SetLinkedModeStateForTab` (`0xa020880`) | relabelled "Open new-tab links in a new pane" in every split, for its new meaning (`split-links.md`) |

**Which ⋯.** Edge's menu knows only "first pane or not". `ToggleContextMenu`
builds it with the bar's `bool` at `ControlButtons+0x360` (the ctor's
`not_first`), or, once set, the bar's callback (`+0x368`):
`EdgeContentsContainerView::IsSplitWindow` (`0xdccbd70`), true for every
container after the first in its parent's children. So every ⋯ but the first
pane's runs the "last tab" variant of items 2 and 3. Our grown panes' bars get
the same callback through `SetControlButtons`.

**Leaving items out.** `PopulateMenu` makes one item view per model item
(`MenuModelAdapter::AddMenuItemFromModelAt`) and attaches the toggle to id 6,
so an item removed from the model before it runs never shows. The
`BuildMenuModel` detour walks the model from its last item:
`SimpleMenuModel::GetItemCount` (`0x28cf330`), `GetCommandIdAt` (`0x342b5b0`),
`RemoveItemAt` (`0xb65da80`), and `SetLabel` (`0xb65dcce`) for the toggle. All
four CHECK the index.

**The copy.** With the ⋯'s pane known, the `ExecuteCommand` detour runs item 2
itself: `chrome::AddAndReturnTabAt` (as the tab offer does, `split-offer.md`)
with the pane's `WebContentsImpl::GetLastCommittedURL` (`0x10ff1a0`, the URL
Edge's copy reads), in the background at the end of the strip, where Edge's
copy lands.

## Building the row

| step | function | rva | what we do |
|---|---|---|---|
| remember the pane | `ControlButtons::ToggleContextMenu(bar, …)` | `0xa011d00` | map the bar to its MCV and pane |
| add the item | `SplitWindowControlMenu::BuildMenuModel(this)` → model | `0xa017fbc` | after Edge's items: a separator, then `AddItemWithIcon` (`0x2b5ea80`) with id `0x10f1e`. The detour must **return the model**: `Run` passes it to `PopulateMenu`, and a void detour crashed Edge. |
| add the d-pad | `SplitWindowControlMenu::PopulateMenu(this, MenuItemView* root, model)` | `0xa017f24` | find the row (`MenuItemView::GetMenuItemByID`, `0xb25e012`) and add a plain `views::View` (ctor `0x101de7a`, `0x338` bytes) holding four `ControlButtons::Button`s (ctor `0xa01109a`, `0x640` bytes), each placed in its cell with `SetBoundsRect` |

The icon is an `ImageModel::FromVectorIcon` (`0xfc8dfc`), called as `(out,
const VectorIcon&, ColorVariant by value, 0, nullptr)`. The `ColorVariant` is
built from ColorId `0x172`, the one Edge's own split ⋯ items use.

## Making the row tall enough

`MenuItemView::GetChildPreferredSize` (`0xb261fa0`) takes a row's **height from
its icon view alone** (`MenuItemView+0x418`). Every other child only adds width.
So:

1. the icon view's preferred size is set to the d-pad's height;
2. `MenuItemView::ChildPreferredSizeChanged(this, child)` (`0xb25c8d0`) drops
   the cached dimensions, so the row is measured again.

The taller row lays out wrong. `MenuItemView::CalculateProposedLayout`
(`0xb25f350`) does two things:

- through `EdgeCalculateIconLayouts`, it centres the icon on a *standard*
  row's height;
- it pins other children to the row's top.

The label, though, sits in the middle. Our detour edits the returned
`views::ProposedLayout` and centres the icon (at its natural height) and the
d-pad in the row. Note the `this` it receives: the method overrides
`views::LayoutDelegate`, so `this` is that base, at `MenuItemView+0x338`, not
the item.

```
ProposedLayout { gfx::Size host_size;                 // +0x0 (height +0x4)
                 vector<ChildLayout> {data +0x8, size +0x10, capacity +0x18} }
ChildLayout    { View* child; bool visible; gfx::Rect bounds (+0xc); SizeBounds }  // 0x30 bytes
```

The vector uses libc++'s size-based layout. This layout is confirmed in
`vector<ChildLayout>::emplace_back` (`0x8ad999a`), which indexes
`data + size * 0x30`.

## The arrows' clicks

A menu forwards a press on a child view to that view, the way Chrome's zoom
buttons work. So a click on an arrow **never becomes a menu command**:
`ShouldExecuteCommandWithoutClosingMenu` and `ExecuteCommand` don't run for it.
The arrows are taken from the `views::Button` handler detours that already
serve the grip:

1. On `Button::OnMouseReleased`, with the button still under the mouse
   (`View::IsMouseHovered`, `0x2893f54`), the detour posts the direction to
   the DLL's UI-thread channel (`Command::kMenuArrowBase`).
2. The channel closes the menu with `MenuController::Cancel(controller, kAll,
   0)` (`0xb263dd0`, controller from `GetActiveInstance`, `0xb263d7e`). This
   is exactly how `MenuRunnerImpl::Cancel` calls it.
3. The channel then adds the pane.

Posting matters. Closing the menu destroys the arrow, so it must not happen
inside the arrow's own release handler.

A click on the row itself (off the arrows) keeps the menu open: the
`ShouldExecuteCommandWithoutClosingMenu` detour (`0xa018ab0`) returns true for
our id. That function is **identical-code-folded** and shared with other menus
(e.g. the downloads bubble), so the detour passes every other id straight to
the original. `ExecuteCommand` (`0xa0186c0`) does nothing for our id.

## The arrow icons

↑ and ↓ are Edge's `kArrowUpIcon` / `kArrowDownIcon` (`0x120bb7b0` /
`0x120bb778`). Edge's ← → (`kArrowBackIcon` / `kArrowForwardIcon`) are drawn
differently, so ← and → are `kArrowUpIcon` turned a quarter (`icons.h`): its
path is copied into Edge's heap and every point rotated about the canvas.

The vector icon ABI:

- `VectorIcon {reps, n, name}`;
- `VectorIconRep {path, size}`;
- 4-byte elements: a command id or a float argument.

The commands the up arrow uses:

| command | id |
|---|---|
| `NEW_PATH` | 0 |
| `MOVE_TO` | 7 |
| `R_ARC_TO` | 10 |
| `LINE_TO` | 11 |
| `R_LINE_TO` | 12 |
| `V_LINE_TO` | 15 |
| `CLOSE` | 27 |
| `CANVAS_DIMENSIONS` | 28 |

A `V_LINE_TO` is written back as an absolute `LINE_TO`, since a vertical line
turns into a horizontal one. If the path ever uses a command the turn doesn't
know, the back / forward icons are used instead.
