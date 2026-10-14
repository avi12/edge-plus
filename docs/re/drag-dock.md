# Dragging a pane to dock it (native split)

How a pane of the N-pane native split is dragged to a new position: the
open-source approaches it copies, the rules adopted, and the Edge internals the
DLL uses. Code: `app/native-split/src/` — `pane_tree.h` (tree edits), `dock.h`
(drop zones), `grip.h` (the grip button), `drag.h` (the session), `overlay.h`
(the preview).

## What open-source docking layouts do

Sources were read at: dockview `dc5ddaf`, react-mosaic `a189cd9`, FlexLayout
`0ae1a6c`, golden-layout `b053a23`, VS Code `main` (grid + editor drop target),
Zed `pane_group.rs`, i3 / sway.

| Concern | dockview | react-mosaic | VS Code | FlexLayout | golden-layout |
| --- | --- | --- | --- | --- | --- |
| Pane edge zone | 20% band | 30% strips, grow to 50% on hover | 10% edge, then thirds | diagonals outside a 25–75% centre box | 25% sides, halves in the middle |
| Whole-layout edge | 10 px, checked first (capture phase) | 10 px strips on top | none | 10 px, parent checked first | 50 px, smallest area wins |
| Centre | merge as tab | swap (optional) | merge | tab | header only |
| Preview | half of the pane | the hovered strip | half of the group | half of the pane | the strip |
| Commit | remove, re-find target, insert | remove, insert 50/50 | remove, insert | halve target / wrap | wrap 50/50 |

Tree edits all share one shape: **remove the dragged pane first, then look the
target up again by id** (never by a stored path, which the removal may have
invalidated), then insert. After an edit a split with one child collapses into
its parent, and a child split along the same axis as its parent is spliced into
it with its weights scaled by its slot (react-mosaic, VS Code), so on-screen
sizes are kept.

## Rules adopted

**Tree** (`pane_tree.h`), canonical after every edit: each split has ≥ 2
children, no split shares its parent's axis, weights are > 0 and sum to the
child count (1.0 = an average share).

- *Beside a pane* (`MovePane`): same split along that axis → a pure reorder
  (the pane keeps its size); otherwise remove, then `InsertBeside`. Joining a
  split along the drop axis keeps an equal row equal and only halves the target
  in a resized row (VS Code's `Sizing.Auto`); a different axis wraps the target
  50/50 in its own slot.
- *Outer edge of the whole split* (`MovePaneToRootEdge`): a root along the other
  axis is wrapped 50/50, so the pane takes the full half; a root along the same
  axis gains it as an end child with an average share.
- *Centre* (`SwapLeaves`): the two panes trade places; sizes stay with the
  positions.
- Removal drops the pane's weight, so its siblings grow in proportion.

The two scenarios this was built for:

| Start | Drop | Result |
| --- | --- | --- |
| `H[1 2 3 4]` | 4 on the bottom edge of 3 | `H[1 2 V[3 4]]` — thirds across, 3 and 4 at equal heights |
| `H[1 2 3 4]` | 4 on the bottom edge of the split | `V[H[1 2 3] 4]` — 4 takes the bottom half, 1–3 the top half in thirds |

**Zones** (`dock.h`), all lengths in DIPs of the split:

1. A pointer up to 48 DIP outside the split is clamped in; farther out is no
   target.
2. The split's outer band first: within 16 DIP of an outer edge (nearest edge
   wins), held until 48 DIP in.
3. Otherwise the pane under the pointer: its centre box (25–75% on both axes)
   swaps; outside it, the pane's diagonals pick the edge (no dead corners); an
   active edge holds until 40% into the pane. The dragged pane offers nothing.
4. A drop that would change nothing (a no-op edit) shows nothing and, for the
   outer band, falls through to the pane under the pointer.

The **preview is the outcome**: `Resolve` applies the edit to a copy of the tree
and lays it out, and the preview is the dragged pane's rect in that result —
not the "half of the target" approximation the libraries draw. It eases between
targets over 90 ms, but not on first appearance (VS Code).

## Edge internals used (msedge.dll 155.0.4283.45)

**The hover bar.** In old-toolbar mode each `EdgeContentsContainerView` owns an
`edge::split_screen::ControlButtons` view (pointer at container `+0x540`). Its
`InitButtons` calls `ControlButtons::AddButton(this, string_id, const
gfx::VectorIcon*, bool clickable)` (rva `0xa0117b6`) for ⋯ (`+0x340`), a hidden
screenshot button (`+0x348`) and X (`+0x350`). `clickable` binds the click to
`HandleButtonClick`; `false` binds nothing, which is what the grip wants (the
press is caught in the window procedure instead). `InitLayout` (rva
`0xa010c8c`) installs a `BoxLayout` and sets the preferred size to
`{32 × visible children, 32}`; re-running it after adding the grip widens the
bar by Edge's own rule. The grip is moved to index 0 with
`View::ReorderChildView`.

**Vector icons.** `gfx::VectorIcon` is `{reps*, reps_size, name*}`; each rep is
`{path*, path_size}`; each path element is a 4-byte command id or float. The
command ids in this build, decoded from `kEdgeSplitWindowMoreIcon` (three dots):
`MOVE_TO = 7`, `R_MOVE_TO = 8`, `R_ARC_TO = 10`, `CLOSE = 27`,
`CANVAS_DIMENSIONS = 28`. The grip draws six `r = 1` dots the same way (a
`MOVE_TO` and two half `R_ARC_TO`s each) on a 16 canvas. The icon lives in
Edge's heap and is never freed, so a grip can never point into an unloaded DLL.

**Coordinates.** `View::parent_` is at `+0x198`; the widget's root view has
none, and its coordinates are the window's client area in DIPs (Edge draws its
own frame, so client = whole window). `View::ConvertPointToTarget(src, dst,
gfx::Point*)` (rva `0xfd8b86`) converts in place. Client pixels ↔ DIPs use
`GetDpiForWindow / 96`. `View::IsDrawn` (rva `0x10d5150`) is true only for a
visible bar, i.e. a hovered pane.

**Input.** A real press on a pane bar never reaches the top-level window
procedure as `WM_LBUTTONDOWN` (only the later moves and the release do — Edge
consumes the press before window dispatch), so the grip cannot be driven from
the subclassed window. It is driven from views instead: `views::Button`'s
`OnMousePressed` / `OnMouseDragged` / `OnMouseReleased` / `OnMouseCaptureLost`
(rva `0xb287ce0` / `0xb287d70` / `0xb287ee0` / `0xb287f00`, shared by every
button, `ControlButtons::Button` included) are detoured; for a grip the press
starts the session and is claimed, so views keeps sending that button the drags
and the release under its own capture. Every other button runs the original. An
event's `root_location_` (`ui::LocatedEvent` `+0xb8`, a `gfx::PointF`) is
already in the window's DIPs. Escape is still caught in the window procedure.

**Safety patches stay for the process lifetime.** The feed guard (`index < 2`),
the accessible-label builder's `int3` at pane index ≥ 2, and `SetActiveIndex`'s
`index < 2` CHECK are byte patches that point at nothing of ours. Restoring them
on unload crashed a still-grown split as soon as Edge built a label for pane
3+, so they are now left in place; only the MinHook detours are removed.

## Testing without a mouse

`fixedge.addpane` wParam `500 + i` shows pane *i*'s bar via Edge's own
`SplitWindowController::ShowControlButton`, and a posted `WM_LBUTTONDOWN` on
its grip reaches the press handler. Posted moves, however, do not turn into
views drag events, so the drag itself is covered by the pure tests
(`dock_test`, `pane_move_test`) and verified with a real mouse.

A test window covered by another window is treated as occluded and its pages
stop painting; launch it with `--disable-features=CalculateNativeWinOcclusion`.
