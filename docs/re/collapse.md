# Collapse: split-button-while-split → keep active pane, eject the rest as standalone tabs

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`). Every `0x…` is an **RVA**; linear = `base + rva`.
Re-verify after an Edge update. Produced with `tools/re/symbols.py`,
`tools/re/disasm.py`, `scratchpad/drange.py`, and a scratch `vtbl.py`/`jt.py`
(vtable + jump-table reader). Builds on `controller.md` and `split-view-abi.md`.

> **Bottom line — the feature already exists natively, as one action value.**
> The toolbar split button, when a split is open, runs
> `edge::split_screen::SplitWindowControllerImplV2::DoActionToSplitWindow`
> (`0xa021020`). That function is a **6-way action dispatch**. **Action `2`** is
> *exactly* collapse-keeping-active: it reads the active WebContents' strip index,
> calls **`TabStripModel::RemoveSplit(splitId)`** (which **dissolves the whole
> `SplitTabCollection`, moving every member tab back to the strip as a standalone
> tab — none are closed**), then **`ActivateTabAt(activeIndex)`** so the active
> pane stays foreground and the view collapses to full window. The only reason the
> toolbar does not *always* do this is that `HandleCloseSplitWindow` passes
> `action = 2 | isFocused`, so when the split content is focused it sends **action
> `3`** (same dissolve, but it re-activates the *last* tab instead of the active
> one). Forcing action `2` is the whole fix. **No tab-closing primitive is on the
> collapse path** — the close primitives (`CloseWebContentsAt`,
> `MaybeCloseCurrentActiveWebContents`) live on *other* action values and must not
> be called.

---

## 0. Call map (RVAs)

```
toolbar split button (while split)
  SplitWindowController::ToggleSplitWindow           0xa0230ee
    -> edge::split_screen::HandleCloseSplitWindow    0xa027675
         activeWC = TabStripModel::GetActiveWebContents(browser->model)   0xf97532
         flags    = IsSplitViewVisibleAndFocused() ? 1 : 0 , then |= 2     => 2 or 3
         controller->vtbl[+0xb0](controller, activeWC, flags)
           == SplitWindowControllerImplV2::DoActionToSplitWindow          0xa021020
                action 2  == COLLAPSE, keep ACTIVE  (desired)
                action 3  == dissolve, keep LAST tab (native, when focused)

DoActionToSplitWindow action 2/3 body:
    GetSplitData()  (controller)                     0xa01fc50  -> SplitTabData*
    SplitTabData::ListTabs()                         0x8184fe0
    TabStripModel::GetIndexOfWebContents(keepWC)     0x10a7d34  -> keepIndex
    TabStripModel::RemoveSplit(model, splitId)       0xa275c18  <<< DISSOLVE, KEEP TABS
        -> TabStripModel::RemoveSplitImpl            0xa26df6a
             -> tabs::TabStripCollection::Unsplit    0x8188592  (MoveTabImpl x N + delete empty coll)
    TabStripModel::ActivateTabAt(model, keepIndex)   0xa26c482  <<< keep that pane foreground

view collapses to full window (driven by the model change):
  BrowserView::MaybeUpdateSplitView                  0x2b58058
    -> MultiContentsView::CloseSplitView             0xc93117a  (clears pane[1], hides resize handle)
```

Primary `SplitWindowController` interface vtable: **RVA `0x11563350`**
(`DoActionToSplitWindow` at slot **`+0xb0`**; `GetActiveWebContents` at `+0x58`;
`IsSplitViewVisibleAndFocused` at `+0x30`; `HideSplitView` at `+0x20`).

---

## 1. Dissolve-but-keep-tabs — the decisive teardown

### `tabs::TabStripCollection::Unsplit` (`0x8188592`)

This is the function that tears a split down **without closing its tabs**. Args:
`rcx` = `TabStripCollection*` (the strip's root collection, i.e.
`TabStripModel+0x10`), `rdx` = `SplitTabId` (16-byte token). Decisive body:

```
0x818866e: call tabs::TabCollection::GetTabsRecursive   ; 0x2c8f98c -> vector of ALL tabs in the split collection
0x8188673: r13 = count<<3                                ; loop bound (handles N, not just 2)
; ---- move every member tab OUT of the split collection, back onto the strip ----
0x8188689: rcx = tabs[i]
0x81886a4: call tabs::TabStripCollection::MoveTabImpl    ; 0x81879ae  (one per tab, i=0..count)
0x81886b3: loop while i != count
; ---- then remove and delete the now-empty SplitTabCollection ----
0x81886c6: call tabs::TabStripCollection::RemoveTabCollectionImpl  ; 0x8187d7a
0x81886e5: call [guard] coll->vdtor(1)                   ; delete the empty SplitTabCollection
```

`MoveTabImpl` **relocates** each tab to a standalone position in the strip; it
does **not** destroy the WebContents. There is **no `CloseWebContentsAt` / tab
close anywhere in `Unsplit`**. So dissolving a split returns every member as a
normal tab. This is the SSOT "keep the tabs" primitive.

### `TabStripModel::RemoveSplitImpl` (`0xa26df6a`)

The model-level wrapper that calls `Unsplit`. Args: `rcx` = `TabStripModel*`,
`rdx` = `SplitTabId`, `r8d` = bool.

```
0xa26dfc0: call TabStripModel::GetTabsAndIndicesInSplit  ; 0xa269a50 (snapshot members+indices)
0xa26dfc5: rcx = [model+0x10]                            ; the TabStripCollection
0xa26dfd8: call tabs::TabStripCollection::Unsplit        ; 0x8188592  <<< the dissolve
; then rebuild selection model + fire TabStripModelChange / TabStripSelectionChange observers
```

### `TabStripModel::RemoveSplit` (`0xa275c18`) — public entry

```
0xa275c5b: [model+0x108] = 1                              ; model-update reentrancy guard
0xa275c70: call TabStripModel::NotifyInactiveSplitTabWillBecomeHidden ; 0xa275cf4
0xa275c86: call TabStripModel::RemoveSplitImpl(model, splitId, 0)     ; 0xa26df6a
0xa275c8e: call TabStripModel::CompleteModelUpdateTransaction
0xa275c93: [model+0x108] = 0
```

Takes `rcx` = `TabStripModel*`, `rdx` = `SplitTabId` (16 bytes, by value in xmm0).
**This is the one function to call to dissolve a split while keeping its tabs.**

### Canonical confirmation — the extension API

`extensions::TabsUnsplitFunction::Run` (`0xeff1750`) resolves the split id, checks
`IsTabStripEditable`, gets the `TabListInterface`, and calls its **vtable `+0x100`
`Unsplit(splitId)`** virtual — which routes to `TabStripModel::RemoveSplit`. So
"unsplit, keep tabs" is the documented contract of this whole chain.

### Controller convenience — dissolve every split in the window

`SplitWindowControllerImplV2::UnSplitAllSplitTabs` (`0xa021bc0`): walks
`TabStripModel::count()` (`0x107afae`), `GetSplitForTab(model,&out,i)`
(`0x2b2a682`) for each index, dedups the `SplitTabId`s into a `std::set`, then for
each one calls `TabStripModel::RemoveSplit` (`0xa021cad`). For a single-split
window this is a one-call "dissolve, keep all tabs". It does **not** choose which
tab stays active (no `ActivateTabAt`), so prefer action `2` when you must keep the
active pane foreground.

---

## 2. Remove a single tab from a split — there is no "keep the split for the rest"

Native splits are an all-or-nothing `SplitTabCollection`. **There is no primitive
that pulls one member out and leaves a smaller split.** The relevant functions all
dissolve the *whole* split:

| function | RVA | behavior |
|---|---|---|
| `TabStripModel::RemoveSplit` | `0xa275c18` | removes the **whole** split by id (keep tabs) |
| `TabStripModel::MaybeRemoveSplitsForMove` | `0xa27ce6e` | when a tab is moved and the split becomes invalid, calls `RemoveSplitImpl` (`0xa27d05a`) — dissolves |
| `TabStripModel::MaybeRemoveSplitsForUpdate` | `0xa274864` | when members go missing (`cmp present_count, coll[+0x18]; jb`), calls `NotifyInactiveSplitTabWillBecomeHidden` + `RemoveSplitImpl` (`0xa274a38`) — dissolves |
| `tabs::TabStripCollection::MoveTabImpl` | `0x81879ae` | lowest-level "move one tab out of a collection"; used *inside* `Unsplit`. Calling it on a live split by itself leaves the split inconsistent until a `MaybeRemoveSplits*` validity pass runs. |

**Consequence for collapse:** you do **not** eject panes one-by-one. `RemoveSplit`
dissolves the entire `SplitTabCollection` at once; *all* members (active and
others) land back on the strip as standalone tabs; you then pick which one stays
foreground. This is true for N panes as well — `Unsplit`'s `GetTabsRecursive` loop
moves out every member regardless of count.

---

## 3. Active-pane identity — which tab to keep

| datum | where | evidence |
|---|---|---|
| active tab | `TabStripModel+0x98` | `TabStripModel::GetActiveTab` (`0xfd2e60`) is literally `mov rax,[rcx+0x98]; ret` |
| active WebContents | `GetActiveWebContents(model)` | `0xf97532`; also `GetActiveWebContentsWithSplitWindow` `0xfb9eda`; controller's own `GetActiveWebContents` vtbl`+0x58` = `0xa020730` |
| active tab's split id | `TabInterface::GetSplit()` | virtual **vtable `+0x118`**, returns `optional<SplitTabId>` (has-value at `out+0x10`, 16-byte id at `out+0`); used in `BrowserView::ShowSplitView` and `MaybeRemoveSplitsForUpdate` |
| split id by strip index | `TabStripModel::GetSplitForTab(model,&out,i)` | `0x2b2a682` (used by `UnSplitAllSplitTabs`) |
| `SplitTabData*` for an id | `TabStripModel::GetSplitData(model,id)` | `0xa2677a4` → `GetSplitTabCollection(model[+0x10],id)` (`0x81882ea`) then `[coll+0x90]` |
| controller's current `SplitTabData*` | `SplitWindowControllerImplV2::GetSplitData` | `0xa01fc50` (no id arg — finds the active split itself; used by `DoActionToSplitWindow`) |
| active-pane visual flag | `ContentsWebView+0x450` | per `split-view-abi.md` / `ctor.md` |
| MCV active-container index | `MultiContentsView+0x420` (int) | `CloseSplitView` `movsxd rax,[mcv+0x420]`; `SwapContentsInSplitView` normalizes it to 0 |

The model-active tab (`model+0x98`) is the "active side". **Action `2` keeps
exactly this tab** because it calls `GetActiveWebContents` (model-side) for the
WebContents it preserves.

---

## 4. Full-window resize of the kept pane — automatic

Once the split is removed, the active tab is no longer in a `SplitTabCollection`,
so the next UI update collapses the view:

`BrowserView::MaybeUpdateSplitView` (`0x2b58058`) → `MultiContentsView::CloseSplitView`
(`0xc93117a`):

```
0xc931182: if !resizeArea(mcv+0x3f0)->GetVisible() return   ; only act if currently split
0xc931198: if (mcv+0x420 active index != 0) SwapContentsInSplitView  ; move kept content to container[0]
0xc9311cb: ContentsWebView::SetWebContents(containers_[1][+0x3a8], NULL)  ; *** clear pane 1's web view ***
0xc9311f5: [guard] containers_[1]->...(NULL)                 ; detach second pane's contents
0xc931204: EdgeMultiContentsResizeArea::SetVisible(mcv+0x3f0, 0)         ; *** hide the separator ***
0xc93120c: MultiContentsView::UpdateContentsBorderAndOverlay
; (plus SadTab reinstall housekeeping)
```

With the second pane cleared and the resize handle hidden, the remaining single
container is laid out to fill the whole content region (width **and** height) by
`CalculateProposedLayout`'s single-pane path — matching this repo's landed commit
*"collapse fills the whole window (width and height)"*. No leftover panes remain.
`ActivateTabAt(keepIndex)` (`0xa26c482`, args `rcx`=model, `edx`=index,
`r8`=`&{reason=6, TimeTicks}`) guarantees the kept tab is the foreground tab.

---

## 5. `DoActionToSplitWindow` — the full 6-way action map (so we pick the right one)

`SplitWindowControllerImplV2::DoActionToSplitWindow(controller, keepWC, action)`
(`0xa021020`). `rdx`=`keepWC` (must be non-null), `r8d`=`action` (0–5, jump table
at RVA `0x1260406c`). Guard `GetSplitData()` must return non-null (there is a split).

| action | target | what it does | safe for collapse? |
|---|---|---|---|
| **2** | `0xa021185` | `GetIndexOfWebContents(keepWC)` → `RemoveSplit(splitId)` → `ActivateTabAt(thatIndex)` — **dissolve, keep the ACTIVE pane foreground** | **YES — this is collapse** |
| 3 | `0xa02113b` | same, but `keepWC` is overwritten with `ListTabs().back()` first → dissolve, keep the **last** tab foreground | keeps tabs, wrong pane |
| 4 | `0xa021107` | `GetIndexOfWebContents(keepWC)` → **`CloseWebContentsAt(idx, 1)`** — **CLOSES the active tab** | **NO — closes a tab** |
| 5 | `0xa0210e1` | `TabStripModel::ReverseTabsInSplit(splitId)` (`0xa2737b4`) — swap sides | n/a (no teardown) |
| 1 | `0xa021243` | build `OpenURLParams` on `keepWC`'s URL, `MaybeShareCopilotHostBetweenTabs`, open (duplicate-into-split flow) | n/a |
| 0 | `0xa0211f9` | `keepWC = ListTabs().back()`, then the action-1 open flow | n/a |

Both collapse-safe actions (2 and 3) go through `RemoveSplit` and **never** touch
`CloseWebContentsAt`. The tab-closing actions are **4** and the separate method
**`MaybeCloseCurrentActiveWebContents`** (`0xa021650` → `CloseWebContentsAt`
`0xa26f916`) — **do not call these for collapse.**

### Recommended call sequence for the injected DLL (UI thread)

The goal (keep the active pane full-window, eject every other pane as a standalone
tab) is precisely **action 2**. The native toolbar already runs `DoActionToSplitWindow`
with action `2 | isFocused`; the only defect is that a *focused* split sends action
`3` (keeps the last tab, not the active one). Two equivalent fixes, cheapest first:

1. **Force action 2 (preferred).** Hook `HandleCloseSplitWindow` (`0xa027675`) and
   drop the focused bit so the vtable`+0xb0` call always receives `action = 2`
   (i.e. replace the `or ebx, 2` / `IsSplitViewVisibleAndFocused` result with a
   constant `2`). Equivalently, hook `DoActionToSplitWindow` (`0xa021020`) and
   coerce incoming `action == 3 → 2`. This reuses 100% of the native dissolve +
   activate + full-window path and keeps N-pane behavior for free.

2. **Call it directly from our button handler** (bypass the native toggle fork):
   ```
   model      = <TabStripModel*>                 // controller+0x48, or browser->model
   activeWC    = TabStripModel::GetActiveWebContents(model)          // 0xf97532
   controller  = <SplitWindowControllerImplV2*>  // the window's split controller
   controller->DoActionToSplitWindow(controller, activeWC, /*action=*/2)  // vtbl+0xb0 @ 0xa021020
   ```
   which internally performs, in order:
   `GetSplitData()` → `ListTabs()` → `keepIndex = GetIndexOfWebContents(activeWC)`
   → `RemoveSplit(model, splitId)` (dissolve: `Unsplit` → `MoveTabImpl × N`, tabs
   survive) → `ActivateTabAt(model, keepIndex)` → view auto-collapses via
   `MaybeUpdateSplitView`/`CloseSplitView` to full window.

3. **Manual equivalent** (if we ever need to keep a *different* pane, or to drive
   it from a per-leaf menu in the future tree model): compute `keepIndex` **before**
   teardown, then `RemoveSplit(model, activeSplitId)` (`0xa275c18`) + `ActivateTabAt`
   (`0xa26c482`). Get `activeSplitId` from `GetActiveTab()` (`model+0x98`) →
   `TabInterface::GetSplit()` (vtbl`+0x118`), or `GetSplitForTab(model,&out,index)`
   (`0x2b2a682`). This is what `UnSplitAllSplitTabs` (`0xa021bc0`) does per split,
   minus the activate step.

**Which entry to hook:** the toolbar button already reaches
`SplitWindowController::ToggleSplitWindow` (`0xa0230ee`) →
`HandleCloseSplitWindow` (`0xa027675`) when a split is open. Hooking
`HandleCloseSplitWindow` to pin the action to `2` (option 1) is the smallest,
lowest-risk change and cannot close tabs, because action `2`'s only teardown call
is `RemoveSplit`.

---

## 6. Honest uncertainties

- **Index stability across `RemoveSplit`.** Action 2 computes `keepIndex`
  *before* `RemoveSplit` and trusts it for `ActivateTabAt` afterward. Native relies
  on split members being contiguous so the active tab's absolute index is
  preserved by `Unsplit`/`MoveTabImpl`. Replicate the native order exactly
  (index → RemoveSplit → activate); do not recompute mid-way.
- **Action enum names.** The 0–5 values are read off the jump table, not from
  symbol names. The behaviors in §5 are from disassembly; the human-facing enum
  (e.g. an `edge::split_screen` "SplitWindowAction") was not located.
- **N-pane future.** `RemoveSplit`/`Unsplit` dissolve a whole `SplitTabCollection`
  of any size (the `GetTabsRecursive` loop is N-generic), so collapse scales to the
  DLL-managed tree. But `MaybeRemoveSplitsForMove`/`ForUpdate` (`0xa27ce6e` /
  `0xa274864`) auto-dissolve a split the moment a member looks missing — relevant
  if the DLL ever tries to mutate split membership incrementally rather than
  rebuild. Not a concern for collapse (we want full dissolve).
- **`CloseSplitView` second-pane detach.** The `[guard] containers_[1]->vtbl+0x98(NULL)`
  at `0xc9311f5` is read as "detach the second pane's contents"; the exact method
  behind that slot was not named. The visible effect (pane cleared, separator
  hidden, single full-window content) is confirmed by the surrounding calls.
