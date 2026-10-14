# The pane bar's ⋯ menu — the "Add pane" d-pad row

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**; their one home
in code is `app/native-split/src/offsets.h`.

The ⋯ button in a pane's hover bar opens `SplitWindowControlMenu`, not the
toolbar's `SplitTabMenuModel` (`split-menu.md`). We append one row to it:
Edge's split icon, the label "Add pane", and a d-pad of ↑ ← → ↓ buttons. The
row itself does nothing; an arrow adds a pane beside the pane whose ⋯ it was,
in that direction.

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
