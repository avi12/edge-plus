# Closing one pane of a grown split (native split)

What Edge does when a pane's X is clicked, why that is wrong past two panes,
and what the DLL does instead. Code: `split_ops::ClosePane`,
`split_data::CloseMember`, `pane_tree.h`'s `RemovePane`, the
`ControlButtons::HandleButtonClick` detour in `hooks.h`.

## What Edge does (msedge.dll 155.0.4283.45)

The X of a pane's hover bar runs `ControlButtons::HandleButtonClick(bar,
button)` (rva `0xa011ae0`). For the close button (`bar+0x350`):

- pane 0 (no earlier container sibling, flag `+0x360` clear):
  `DoActionToSplitWindow(activeWebContents, 4)`;
- every later pane: `SplitWindowController::HideSplitView(true)` (vtable `+0x20`,
  rva `0xa0201f0`), which takes the split's **last** member
  (`SplitTabData::ListTabs().back()`) and calls
  `TabStripModel::CloseWebContentsAt(index, CLOSE_USER_GESTURE = 1)`.

Two problems once a split has three or more panes: the X of pane 2 of 4 closes
pane 4's tab, and closing **any** member dissolves the whole split (observed: a
4-pane split became the single remaining active tab).

## What the DLL does instead

For a split of three or more (its bar has a grip), the X closes exactly its own
pane:

1. Snapshot every container's on-screen rect and pick who stays active (the
   active pane, or the closing pane's neighbour in reading order).
2. `CloseMember`: read the members (`GetTabsAndIndicesInSplit`), find the
   closing and the next-active member **by the WebContents their tabs show**
   (not by assumed order), keep the split's visual data,
   `RemoveSplitImpl` (transactional, every tab kept), `CloseWebContentsAt` on
   the one tab, then re-split the remaining members in strip order under a
   fresh id (`AddToSplitImpl`, as `GrowSplit` does) and `ActivateTabAt` the
   next-active member.
3. `RemovePane` on the tree: the closed leaf's siblings share its space in
   proportion (a resized ratio is kept; a sibling in another split is
   untouched), and leaf ids above it drop by one, because Edge re-feeds the
   containers after it one slot earlier.
4. Each re-fed container is first put back (`View::SetBoundsRect`, rva
   `0xfd8234`) where its tab was on screen, the animation flight is restarted,
   and a relayout eases every pane to its new place.

A two-pane split is left to Edge (its X there means the split's own close).

## Dividers and equal sizes

Every seam of a grown split's tree (N - 1 for N panes) gets one of Edge's own
divider views, `EdgeMultiContentsResizeArea` (ctor rva `0x2ce11b0`, 0x358
bytes, `(this, MultiContentsView*)`): the two-pane split's handle, with its
hover pill, and a resize cursor that turns vertical when the view is taller
than wide (`EdgeTwoDimensionalResizeArea::GetCursor` compares its own width and
height). They are created outside layout, kept above the panes in z-order, and
placed over the seams on every layout pass; Edge's single two-pane handle
(`MCV+0x3f0`) is hidden. A drag is taken from the class's own mouse handlers
(`EdgeTwoDimensionalResizeArea::OnMousePressed / Dragged / Released /
CaptureLost`, rvas `0xb1fac20 / 0xb1facb0 / 0xb1facf0 / 0xb1fad50`); Edge's own
handle still runs the originals. `ResizeSeam` trades the dragged distance
between the two children of that seam only, so every other proportion holds,
and the weights persist through window resizes, moves and closes.

Double-clicking a divider gives every pane of the split it divides an equal
share again, eased into place, as Edge's own handle does for its two panes. The
release of the second click reports `ui::MouseEvent::GetClickCount` (rva
`0xb631a18`) as 2. Only that split changes; resizes elsewhere in the tree hold.
