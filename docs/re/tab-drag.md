# Drag a tab into a split

Target: `msedge.dll` 155.0.4283.45. All addresses are **RVAs**; their one home
in code is `app/native-split/src/edge/offsets.h`.

**What the user does:**

1. Drag one tab in the list. Over the list it is Edge's own drag: the tab
   follows the pointer and the others make room. The split that was on screen
   when the tab was grabbed is *picked*.
2. Hover a split's tab: Edge's drag pauses there, that split shows and is
   picked, and the preview puts the tab at the split's right edge. Releasing
   there adds it so.
3. Move off the list, into the window: the picked split shows, and the docking
   preview shows where the tab would land.
4. Release: the tab becomes a new pane there.

A pane dragged out of its split by its grip is the same drag, its own split
picked from the start. "In the window" is the root view of the tab list's own
widget, so a drag in any window works.

**The split on screen at the grab.** The press activates the tab before the
drag starts, so it is the active page before the dragged one. Switching tabs
does not go through `MultiContentsView::SetWebContentsAtIndex`, but a layout
of the split view always follows, so the `CalculateProposedLayout` detour
keeps the last two active pages (`tabdrag::OnLaidOut`).

**A plain tab on screen at the grab** is picked too, but counts only while Alt
is held (`tabdrag::DockTarget`): off the list the preview then splits that one
page left / right / top / bottom (a one-leaf tree, `dock::ResolveIncoming`),
and the drop makes the two tabs a new split. Without Alt the drag stays Edge's,
so tearing a tab off is unchanged. Edge lays out a two-pane split itself, so
the drop's two-leaf tree becomes the split's layout (`SplitTabVisualData` +0:
0 side by side, 1 one above the other) and its member order (the strip order:
the dragged tab is moved before the shown one for left or top), in
`split_ops::CreatePair` and `split_data::CreateSplitWith`. A script holds Alt
with channel 954 (lParam 1, then 0).

**Alt over the tab list** holds Edge's drag there too, between targets as well
(its reordering would slide the next tab away from under the pointer), and a
plain tab under the pointer is picked (`tabdrag::PickPlainTabUnder`): its page
shows, and letting go there puts the dragged tab on its right; moving on onto
the page picks the side. Each drag logs what it starts with picked and whether
Alt was held.

With nothing picked, or out of the window, the drag is Edge's own (it tears the
tab off). Into a split, Alt changes nothing: until 2026-10-24 it was the switch
to docking, and a pane's drag needed none, so the two drags behaved
differently; its key events are still dropped so a held Alt never ends a drag.
Code: `src/chrome/tabdrag.h`.

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

| detour | dock mode | otherwise |
|---|---|---|
| `EventTracker::OnEvent` | Alt's key events are dropped, so Alt never ends the drag | same |
| `ContinueDragging` | Not called. The picked split is activated; over it, `dock::ResolveIncoming` drives the preview. Returns 0 (the drag lives on). | original; over the list we hit-test its tabs to pick a split; leaving dock mode re-activates the dragged tab first |
| `EndDrag` | A completed release over a drop target remembers the plan, runs the original, then **posts** the drop to the UI-thread channel (`kTabDrop`) | original |

**Dock mode** means: exactly one tab is dragged, that tab is not in a split, a
split is picked, and the pointer is in the window but off the tab list.

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
`src/chrome/tabdrag.h`; the hand-off point in `drag.h` (`IsReadyForTabList`).

**What the user does:** drags a pane of a grown split by its grip onto the
vertical tab list. The pane's tab leaves the split's row at once and follows
the pointer through the list, the other tabs making room, exactly as a tab
dragged within the list (or in from another window) does. Releasing there
leaves it a tab of its own at that place. Back over the split the drag docks
as any tab's drag does (the split shows again, the preview follows the pointer,
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
some modes. While a split is picked, the `ContinueDragging` detour treats any
point in the window outside the strip's plain bounds as docking and does not
call the original, so Edge never sees a point there that would tear the tab
off; out of the window it does, and tears it off. Docking activates the picked
split (one of its panes' tabs); going back over the list re-activates the
dragged tab first, since Edge moves the selected tabs.

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

## A pane left blank by the tab that came back

Found and fixed on 156.0.4314.42 (RVAs below are that build's).

**What Avi saw.** A split of three: two panes stacked on the left, one on the
right. He dragged the bottom-left pane out onto the tab list, so it became a
tab of its own. Then he Alt-dragged that tab back to the same place. The split
re-formed, but the pane stayed blank until focus moved.

**Why.** Pane 2 kept its old page after the page left the split:

1. **The drag-out.** `CloseMember` dissolves the split (Edge has no
   "remove one member"). `MultiContentsView::CloseSplitView` (`0xa66e4f8`)
   then runs. It empties pane 1 with `ContentsWebView::SetWebContents(null)`
   and hides it. It never touches panes 2 and up, which are ours. The split is
   re-made from the two left, and Edge feeds panes 0 and 1. Pane 2 is laid out
   hidden, but its web view still holds the page that left (`web_contents_`,
   `CWV_web_contents`).
2. **The Alt-drag.** Grabbing the tab activates it. Edge moves its page into
   pane 0, a native view having one parent. Hovering the split then puts the
   split's page back in pane 0, which takes the tab's page out again.
3. **The drop.** `GrowSplit` re-makes the split with the tab as member 2, and
   `BrowserView::ShowSplitView` feeds `SetWebContentsAtIndex(page, 2)`.
   `views::WebView::SetWebContents` returns at once when handed the page it
   already holds, so the page is never attached to pane 2's native view host.

Before the fix the log read `index 2 <- 0x…F800 (was 0x…F800)`. At the drop the
page's visibility was `2` (visible, `WebContentsImpl+0xdf0`) and its
`RenderWidgetHostViewAura::IsShowing` was true. The page was live and visible;
it was just in no pane. That is why showing it again (`WasShown`, the repaint
nudge) could not help. Why a focus change then paints it was not traced.

**The fix** (`hooks.h` `h_close_split_view`, `panes::EmptyGrownContainers`)
does for panes 2+ what Edge does for pane 1. Once `CloseSplitView` has run,
every grown pane is emptied. Every way a split shrinks or goes comes through
there:

- a pane closed or dragged out;
- the split dissolved by Edge or by us;
- another tab shown in its place.

When a split is re-made, Edge feeds every member at once, so no live pane goes
blank (checked live: 4 panes down to 3, then 2, and a plain tab and back). A
pane's page is detached and re-attached on every re-split, as Edge already does
for pane 1.

**Rejected.** Forcing a detach in `h_setwc` whenever a pane is fed the page it
holds. A stale web view's detach runs on the native view wherever it lives now
(`NativeViewHost::Detach` clears its host-view and accessibility-parent
properties). Emptying the pane while the page is still nowhere else avoids
that.

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
