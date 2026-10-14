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
