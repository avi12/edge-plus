# Alt + drag a tab into a split

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**; their one home
in code is `app/native-split/src/offsets.h`.

**What the user does:**

1. Drag one tab in the strip, holding Alt.
2. Hover a split's tab: that split shows.
3. Move down over its panes: the docking preview shows where the tab would
   land.
4. Release: the tab becomes a new pane there.

Without Alt, the drag is Edge's own. Code: `src/tabdrag.h`.

## Edge's tab drag

`TabDragController` runs every tab drag:

1. A press on a tab creates it.
2. `TabDragController::Drag(this, const gfx::Point* screen)` (`0xa6165a6`)
   sees each move. It reaches `ContinueDragging` (`0xa617de2`) once the
   pointer is past the start distance. From then on the state at `+0x38` is
   1 (dragging tabs).
3. A release calls `EndDrag(this, reason)` (`0xa616010`), and the controller
   is freed.

| field | meaning |
|---|---|
| `+0x38` | state: 0 = not started, 1 = dragging tabs, 2 = dragging a window, 6 = stopped |
| `+0x58` | the attached `TabDragContext`, a `views::View`. Its vtable `+0x3a0` is `GetTabForContents(WebContents*)` → `Tab*`; `+0x3c0` is `GetTabStripModel()` |
| `+0xe0` / `+0xe8` | `vector<TabDragData>` as `{data, size}`, entries `0x70` bytes apart; entry `+0x10` is the `WebContents*` |

Three behaviours of `ContinueDragging` and its key watcher shape the feature:

- **It moves the selected tabs.** `MoveSelectedTabsTo` follows the
  *selection*, so letting it run after we activate another tab would move
  that tab instead.
- **It tears the tab off.** About 15 DIP below the strip, the tab becomes a
  new window and Windows runs a modal move loop.
- **Any key ends the drag.** The key watcher `EventTracker::OnEvent`
  (`0xa61ed50`) lets Escape cancel. Any other key press *or release*
  (`ui::Event+0xc` is 7 or 8) ends the drag in place, Alt included. The key
  code is at `KeyEvent+0xb0`.

## The detours

| detour | Alt mode | otherwise |
|---|---|---|
| `EventTracker::OnEvent` | Alt's key events are dropped, so Alt never ends the drag | original |
| `ContinueDragging` | Not called. We hit-test the strip's tabs; a hovered split tab is activated. Over the shown split, `dock::ResolveIncoming` drives the preview. Returns 0 (the drag lives on). | original; leaving Alt mode re-activates the dragged tab first |
| `EndDrag` | A completed release over a drop target remembers the plan, runs the original, then **posts** the drop to the UI-thread channel (`kTabDrop`) | original |

**Alt mode** means: Alt is down (`GetKeyState(VK_MENU)`), exactly one tab is
dragged, and that tab is not in a split.

**The tab under the pointer** is found by asking the context for each model
tab's view (`GetTabForContents`) and comparing that view's
`View::GetBoundsInScreen` (`0x28ae474`) with the pointer. The pointer is in
screen DIPs. Over the split it goes through `View::ConvertPointFromScreen`
(`0x28940c2`).

**The drop** runs after `EndDrag` has returned, because changing the strip
mid-drag ends the drag. It:

1. re-activates the split;
2. grows it with the dragged tab (`GrowSplit(tsm, tab)`), which first moves
   the tab right after the split, since Edge orders a split's panes by strip
   index;
3. commits the planned tree, in which the tab is the newest leaf.

## A pane's drag handed to the tab list

Target: `msedge.dll` 156.0.4314.42. Code: `TakeOverPaneDrag` and the detours in
`src/tabdrag.h`; the hand-off point in `drag.h` (`IsReadyForTabList`).

**What the user does:** drags a pane of a grown split by its grip onto the
vertical tab list. The pane's tab leaves the split's row at once and follows
the pointer through the list, the other tabs making room, exactly as a tab
dragged within the list (or in from another window) does. Releasing there
leaves it a tab of its own at that place. Back over the split the drag docks
as an Alt drag does (the split shows again, the preview follows the pointer,
releasing adds the tab as a pane there); back over the list it is Edge's drag
again. Esc puts the pane back where it was.

**How a press on a tab starts Edge's drag.** `Tab::OnMousePressed` (`0xa772d50`)
selects the tab, takes the selection (`TabSlotController` vtable `+0x178`,
`GetSelectionModel`, by value), rebuilds the press as a `ui::MouseEvent` in
the tab's coordinates and calls `TabSlotController::MaybeStartDrag` (vtable
`+0x220`) with the tab view, that event and the selection by value (the callee
destroys it). The tab view's `controller_` is at `+0x530`; for a tab of the
list it is the window's `EdgeTabStrip` (its `TabSlotController` base, at
`TabStrip+0x538`). `EdgeTabStrip::MaybeStartDrag` (`0xc9021d0`) adds Edge's
split handling and calls `TabStrip::MaybeStartDrag` (`0xa779350`), which
returns without a drag while:

- the strip animates: `TabStrip::IsAnimatingInTabStrip` (`0x1023c20`), the tab
  container's `BoundsAnimator` or the context's drag-end animation. It always
  does right after the pane's tab left the split's row and was activated, so
  `TabStrip::StopAnimating` (`0x2afe282`, the container's
  `CompleteAnimationAndLayout`) runs first; that also puts the tab where it is
  going;
- `BrowserTabStripController::HasAvailableDragActions` is 0.

It then calls the context's `MaybeStartDrag` (`TabDragContext` vtable
`+0x418`), which creates the `EdgeTabDragController` and `Init`s it. The
event's location is the grab point inside the tab; `Init` turns it into the
drag's start point in screen DIPs, so the first move past the drag threshold
puts the tab under the pointer. The context keeps the controller at `+0x478`
(vtable `+0x3c8`, `GetDragController`).

**The objects.** `BrowserView::tabstrip` (`0x2930564`, `BrowserView+0x670`,
then `+0x5d8`) is the strip of the vertical list too: its
`TabStrip::GetDragContext` (`0x30ba8ee`, `*(TabStrip+0x570)`) is the context
the list's tab views report, and `GetTabForContents` (`+0x3a0`) finds the
pane's tab view. Its bounds in screen DIPs are the tab rows only (in the
throwaway: 249 × 212 DIP for six tabs), not the empty column below them.

**Handing over the mouse.** The grip holds views' mouse capture.
`TabDragController::SetCapture(this, context)` (`0xa783232`) calls
`Widget::SetCapture(context)`, as the attach of a tab dragged in from another
window does; views then sends the strip the moves and the release:

| strip handler | calls |
|---|---|
| `TabDragContextImpl::OnMouseDragged` (`0xa77a270`) | `ContinueDrag`: the event in screen DIPs → `TabDragController::Drag` (`0xa783486`) |
| `EdgeTabDragContextImpl::OnMouseReleased` (`0xa77a290`) | `EndDrag(kComplete)` (vtable `+0x420`) |
| `TabDragContextImpl::OnMouseCaptureLost` (`0xa77a2f0`) | `EndDrag(kCaptureLost)` |

`EndDragReason` is 0 complete, 1 cancel (Escape), 2 capture lost.
`Widget::SetCapture` notifies no one, so the grip's button is told it lost the
capture by hand (`views::Button::OnMouseCaptureLost`), or it would stay drawn
pressed.

**Off the tab list.** Edge's drag tears the tab off into a new window once the
pointer leaves `EdgeTabDragController::DoesTabStripContain` (`0xc9097d0`): the
strip's screen bounds (`GetTabstripScreenBounds`) grown by 15 DIP, or 100 in
some modes. The `ContinueDragging` detour treats any point outside the strip's
plain bounds as docking and does not call the original, so Edge never sees a
point that would tear the tab off. Docking activates the split again (one of
its remaining panes' tabs), as an Alt drag over a split's tab does; going back
over the list re-activates the dragged tab first, since Edge moves the
selected tabs.

**The end.** `EndDrag` complete over a dock target queues the drop (channel
`kTabDrop`) with the planned tree; complete anywhere else leaves the tab where
Edge has it (as releasing a pane outside its split always did); cancel or a
lost capture queues a drop with the split's tree from before, the pane's leaf
renumbered as the newest (`RenumberAsNewest`, since `GrowSplit` appends it),
which puts it back in place.

**Building the press.** `ui::MouseEvent(this, type, const Point* location,
const Point* root_location, TimeTicks, flags, changed_button_flags, const
PointerDetails*)` (`0x2c312e2`, 0x118 bytes, `PointerDetails` 0x48 bytes at
its end, default-built by `0xb78e19a`), `TimeTicks::Now` (`0xfe1850`), type 1
(pressed), flags `0x800` (left button); `ui::Event::~Event` (`0x109ad5a`) ends
it, as `Tab::OnMousePressed` ends its own.

## A crash this exposed

**The crash.** Activating a tab whose pane is one of ours (pane 3 or later)
could crash. On every tab switch, `BrowserView::MaybeUpdateStoredFocusForWebContents`
looks up a focus map for the active pane's container. The lookup is
`MultiContentsView::GetFocusableViewsMapFor` (`0x2b58cc8`), a flat_map at
`MCV+0x4c8` keyed by container. It has entries only for Edge's own two
containers, so it returns null for ours, and the caller dereferences it.

**When it happens.** Only when the new tab has a stored focus, i.e. it was
focused before.

**The fix.** It is that function's only caller, so the detour returns an
empty map for our containers.

## Testing it from a script

**Posted mouse messages alone don't start a tab drag.** Edge:

- takes the drag's capture;
- reads the button state;
- reads the **position from the real cursor**.

**The test harness instead:**

1. Attaches to the test Edge's UI thread input queue (`AttachThreadInput`).
2. Sets `VK_LBUTTON` and `VK_MENU` down in that shared key state
   (`SetKeyboardState`). This reaches neither the system's async key state
   nor any other window.
3. Parks the cursor on each point before posting the move.
4. Restores the cursor at the end.

See `scratchpad/tabdragtest.ps1`.

**One safety rule.** It only leaves the strip once the DLL logged Alt mode.
Edge's own drag would tear the tab off into a window whose modal move loop
waits for a real mouse.
