# The create-split + feed-panes pipeline — `SplitWindowController` → `BrowserView` → `MultiContentsView`

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`). Every `0x…` below is an **RVA**; the linear address
capstone prints is `base+rva` (RVA `0xc93594a` → `0x18c93594a`). Re-verify after
an Edge update. Produced with `tools/re/symbols.py`, `tools/re/disasm.py`, and
two scratch scripts kept in the session scratchpad: `xref.py` (scans every
executable section for `E8`/`E9 rel32` landing on a target and names the call
site) and `drange.py` (uncapped range disassembler). Builds on
`split-view-abi.md` (the view ABI) and `ctor.md` (how the 2 panes are built).

> **Bottom line.** A split is **not** created by the controller. The controller
> (`SplitWindowControllerImplV2`, one per browser window) only *observes* the
> tab strip. The actual split is a **`tabs::SplitTabCollection`** created by
> `tabs::TabStripCollection::CreateSplit` — which is **already N-generic** (its
> only guard is "more than one tab"). When that collection appears, two
> independent `TabStripModel` observers fire: **`BrowserView::OnSplitTabChanged`**
> rebuilds the view, and **`SplitWindowControllerImplV2::OnSplitTabAdded`** wires
> up toolbar/input helpers. The panes get their WebContents from exactly **one**
> function — **`BrowserView::ShowSplitView`** — whose loop is *also* already
> N-generic: it walks `SplitTabData::ListTabs()` and calls
> `MultiContentsView::SetWebContentsAtIndex(mcv, wc, i)` for every tab `i`.
> **Nothing in the data or control path caps panes at two.** The cap is entirely
> in the *view*: `SetWebContentsAtIndex` has a hard `index < 2` CHECK, the
> `MultiContentsView` is physically built with 2 containers + 1 resize area, and
> the geometry math is hardwired to 2 (see `split-view-abi.md`).

---

## 0. The whole pipeline at a glance (RVAs)

```
  user clicks toolbar button
  ToolbarSplitWindowButton::OnButtonPressed      0xa033070
      -> (bound lambda) CommandUpdater::ExecuteCommandWithDisposition
      -> chrome::BrowserCommandController::HandleCommandWithDisposition  0xc9641c0
  SplitWindowController::ToggleSplitWindow        0xa0230ee
      -> edge::split_screen::HandleOpenSplitWindow 0xa027707   (no split yet)
         -> Navigate() to chrome::kEdgeUISplitWindowStartingPageURL  (the "pick a 2nd tab" page)
      -> edge::split_screen::HandleCloseSplitWindow 0xa027675  (split already open)

  -- once two (or more) tabs actually land in a SplitTabCollection --
  tabs::TabStripCollection::CreateSplit           0x818834e   (N-generic; guard: size > 1)
      -> tabs::SplitTabCollection::SplitTabCollection 0x8184e4a
      -> MoveTabImpl  x N   (moves EVERY passed tab into the split collection)

  TabStripModel fires SplitTabChanged to its observers:
    (A) BrowserView::OnSplitTabChanged            0xc93b190   <-- the VIEW path
          -> BrowserView::ShowSplitView           0xc93594a
               loop i in ListTabs():  MultiContentsView::SetWebContentsAtIndex(mcv, wc_i, i)  0xc930f18
               -> MultiContentsView::SetActiveIndex / UpdateSplitVisualData / ShowSplitView
          -> MultiContentsView::CloseSplitView / SwapContentsInSplitView  (unsplit / reorder)
    (B) SplitWindowControllerImplV2::OnSplitTabAdded 0xa021d62 <-- the CONTROLLER path
          -> SplitWindowControllerImplV2::ShowSplitView 0xa01fa70
               wires toolbar + input + focus helpers for the FIRST and LAST tab only

  also drives the view on plain activation:
  BrowserView::UpdateUIForContents 0x2b57f34
      -> BrowserView::MaybeUpdateSplitView 0x2b58058
           -> MultiContentsView::ShowSplitView 0xc930ffc / CloseSplitView 0xc93117a

  controller object is created once per window:
  EmbedderBrowserWindowFeatures::InitPostBrowserViewConstruction 0x2cce838
      -> SplitWindowController::MaybeCreateController 0x2ccea6a
           -> new SplitWindowControllerImplV2(...)  0x2ccfac2   (if migration flag on)
```

---

## 1. Entry / trigger — `MaybeCreateController` and the ctor

### `SplitWindowController::MaybeCreateController` (`0x2ccea6a`)

Called **once per browser window**, from
`EmbedderBrowserWindowFeatures::InitPostBrowserViewConstruction+0xd5`
(`0x2cce90d`) — i.e. at window construction, *not* per split. It builds the
long-lived controller that watches the tab strip.

```
0x2ccea7c: call  features::edge::IsEdgeSplitWindowEnabled      ; feature gate #1
0x2ccea81: test  al, al ; je  <return null>                    ; split disabled -> no controller
0x2ccea89: cmp   dword ptr [rdi + 8], 0                        ; arg2->[+8] must be 0 (state guard)
0x2ccea8d: jne   <return null>
0x2ccea93: call  features::edge::IsEdgeSplitWindowMigrationToUpstreamEnabled ; feature gate #2
0x2ccea9a: je    <legacy path>                                 ; -> old SplitWindowControllerImpl
0x2cceaa0: mov   ecx, 0x110                                    ; sizeof(SplitWindowControllerImplV2)=272
0x2cceaa5: call  operator new
0x2cceab3: call  SplitWindowControllerImplV2::SplitWindowControllerImplV2   ; 0x2ccfac2
0x2cceab8: mov   [rsi], r14                                    ; *out = controller
0x2cceac8: call  [guard_dispatch] (rcx=ctrl, rdx=r8)           ; virtual Init(arg3)
```

**Inputs** (observed): `rcx` = out-slot for the owned controller pointer;
`rdx` (→ `rdi`) = a browser-window features/interface struct — the ctor caches
pointers out of it (see below); `r8` = passed straight into the controller's
first virtual call (its `Init`, which later binds the `BrowserView`). There is
**no SplitTabId / WebContents argument here** — the controller does not create a
split and is not told which tabs to use. It finds splits by observing the tab
strip (§4).

**Two implementations, chosen by a flag.** If
`IsEdgeSplitWindowMigrationToUpstreamEnabled()` is true → `…ImplV2` (the upstream
`MultiContentsView` architecture, the subject of this repo). Else → the legacy
`SplitWindowControllerImpl` (`0xa018b6e` ctor), which uses a different
`SplitContainer`/`ContentsContainerView` layout. On this build the flag defaults
**on** (§5), so V2 is the live path.

### `SplitWindowControllerImplV2::SplitWindowControllerImplV2` (`0x2ccfac2`)

A 272-byte (`0x110`) object. The ctor installs 4 vtables (it is a
`SplitWindowController` + `TabStripModelObserver` + two more interface
subobjects), caches three `raw_ptr`s pulled out of the arg-2 features struct, and
builds its observer list + weak-ptr factory:

```
0x2ccfaeb: call TabStripModelObserver::TabStripModelObserver   ; it IS a tab-strip observer
0x2ccfb47: mov  [rsi + 0x40], rdi                              ; +0x40 = arg2 (features struct)   [raw_ptr]
0x2ccfb4b: mov  r14, [rdi + 0x30] ; mov [rsi + 0x48], r14      ; +0x48 = features->[+0x30]         [raw_ptr]
0x2ccfb71: mov  r14, [rdi + 0x10] ; mov [rsi + 0x50], r14      ; +0x50 = features->[+0x10]         [raw_ptr]
0x2ccfbd2: mov  [rsi + 0xa0], rbx (=rsi+0x28)                  ; self-ref (observer-list head)
0x2ccfc4b: call base::internal::WeakPtrFactoryBase::WeakPtrFactoryBase
```

Member map learned from later methods: `+0x48` = `TabStripModel*`
(used by `OnSplitTabAdded`: `TabStripModel::GetSplitData`/`GetActiveTab`);
`+0x58` = `BrowserView*` (`ShowSplitView` dereferences it to reach the toolbar /
omnibox-overlay); `+0x60` = the lazily-created `ToolbarSplitView`; `+0xc1` =
a "split currently shown" bool. (The `MultiContentsView` itself lives at
`BrowserView+0x6e0` — see §3 — which is a different pointer from the controller's
own members.)

---

## 2. UI trigger — the toolbar button

`edge::split_screen::ToolbarSplitWindowButton::OnButtonPressed` (`0xa033070`)
calls `SplitWindowController::ToggleSplitWindow`. So do three other entries, all
confirmed by `xref.py` on `ToggleSplitWindow` (`0xa0230ee`):

| caller | RVA | what it is |
|---|---|---|
| `ToolbarSplitWindowButton::OnButtonPressed` | `0xa033070` | the toolbar split button |
| `SplitWindowIconView::OnExecuting` | `0xa024930` | omnibox/location-bar icon |
| `EdgeSplitTabsToolbarButton::AcceleratorPressed` | `0xa02fe80` | keyboard shortcut |
| `chrome::BrowserCommandController::HandleCommandWithDisposition+0x1ccd` | `0xc9641c0` | the browser-command route |
| `EdgeMarketingPagePrivateOpenSplitScreenFunction::Run` | `0xf006870` | an extension/private API |

The button's bound click handler (the `Invoker<… split_window_toolbar_view.cc:139 …>::Run`
thunk, `0xa031e40`) doesn't call `ToggleSplitWindow` directly — it resolves a
disposition and runs a **browser command**:

```
0xa031eaa: mov  ecx, [rsi + 0x50]                   ; event flags
0xa031eb2: call ui::DispositionFromEventFlags
0xa031ec7: mov  r9, r15                              ; &event
0xa031eca: call CommandUpdater::ExecuteCommandWithDisposition   ; -> BrowserCommandController -> ToggleSplitWindow
```

`ToggleSplitWindow` (`0xa0230ee`) is the fork:

```
0xa023179: call edge::split_screen::HandleOpenSplitWindow   ; 0xa027707  (no split open yet)
0xa0231bb: jmp  edge::split_screen::HandleCloseSplitWindow  ; 0xa027675  (split already open)
```

**`HandleOpenSplitWindow` does not add a second pane's WebContents.** It builds a
`NavigateParams` for `chrome::kEdgeUISplitWindowStartingPageURL` and calls
`Navigate()` (`0xa027802`) — i.e. it opens the split **starting page** (the
"choose a tab for the other side" surface). The real split forms later, when the
tab model puts two tabs into a `SplitTabCollection`. That is the user-facing
entry; the pane count "2" is **not** decided here.

---

## 3. Feeding WebContents to panes — the ONE caller of `SetWebContentsAtIndex`

`xref.py` over `MultiContentsView::SetWebContentsAtIndex` (`0xc930f18`) finds
**exactly one** call site in the whole image:

```
=== callers of MultiContentsView::SetWebContentsAtIndex (0x18c930f18) ===
  call @ 0x0c935b6c  in  BrowserView::ShowSplitView+0x222
```

### `BrowserView::ShowSplitView` (`0xc93594a`) — the pane-feeding loop

```
0xc935979: mov  rax,[rcx+0x600]; mov rcx,[rax+0x30]  ; rcx = TabStripModel
0xc935995: call TabStripModel::GetIndexOfTab          ; ebp = active tab index
0xc9359bc: call TabStripModel::GetTabAtIndex
0xc9359d4: call [guard] tab->GetSplit()  (vtable+0x118); test [rdi+0x10],1  ; active tab is in a split?
0xc9359fc: call TabStripModel::GetSplitData            ; r14 = split_tabs::SplitTabData*
0xc935a1a: call split_tabs::SplitTabData::ListTabs     ; fills vector at [rsp+0x30]
0xc935a1f: mov  rax,[rdi+8]                             ; rax = tab COUNT
0xc935a23: lea  r13,[rax*8]                             ; r13 = count*8  (loop bound)
0xc935a2b: test r13,r13; jne 0x18c935b3f                ; count>0 -> feed loop

; ---- the feed loop (0xc935b3f) ----
0xc935b43: mov  rbx,[rsp+0x30]                          ; rbx = tabs.data()
0xc935b48: xor  r15d,r15d                               ; i = 0
0xc935b4b: mov  rcx,[rbx+r15*8]                         ; rcx = tabs[i]            (TabInterface*)
0xc935b4f: mov  r12,[rsi+0x6e0]                         ; r12 = this->multi_contents_view_  (BrowserView+0x6e0)
0xc935b59: mov  rax,[rax+0x18]; call [guard]            ; rax = tabs[i]->GetContents()  (WebContents*)
0xc935b63: mov  rcx,r12                                 ; arg1 = MultiContentsView*
0xc935b66: mov  rdx,rax                                 ; arg2 = WebContents*
0xc935b69: mov  r8d,r15d                                ; arg3 = i
0xc935b6c: call MultiContentsView::SetWebContentsAtIndex
0xc935b71: inc  r15; add r13,-8; jne 0x18c935b4b        ; next i, while r13 != 0
```

So **index 0 and index 1 are not special-cased** — the loop iterates
`ListTabs().size()` times and passes `i` straight through. With a 3-tab split it
*would* call `SetWebContentsAtIndex(mcv, wc, 2)`. After the loop it computes the
active pane (`ebp - GetIndexOfTab(firstTab)`) and calls
`MultiContentsView::SetActiveIndex` (`0xc93137e`) and `UpdateSplitVisualData`
(`0xc9310ec`). (`BrowserView+0x6e0` = the `MultiContentsView*`; confirmed because
`UpdateActiveTabInSplitView` at `0xc935b9c` reads the same `[this+0x6e0]` and
calls `MultiContentsView::IsInSplitView`.)

### `MultiContentsView::SetWebContentsAtIndex` (`0xc930f18`) — where the cap lives

```
0xc930f1f: cmp  r8d, 2
0xc930f23: jae  <ud2>                                   ; *** HARD CHECK: index < 2 ***
0xc930f32: cmp  qword ptr [rcx+0x370], rax ; jbe <ud2>  ; also: index < containers_.count (==2)
0xc930f42: mov  rcx,[rdi+0x368]                         ; containers_.data()
0xc930f49: mov  rax,[rcx+rax*8]                         ; containers_[index]
0xc930f4d: mov  rcx,[rax+0x3a8]                         ; container->contents_web_view_  (+0x3a8)
0xc930f54: call ContentsWebView::SetWebContents         ; <-- the WebContents actually lands here
0xc930f59: cmp  ebx, 1                                  ; only index==1 ...
0xc930fdd: mov  rcx,[rdi+0x3f0]                         ;   ... touches the single resize area (+0x3f0)
0xc930fe6: call EdgeMultiContentsResizeArea::SetVisible
```

So each pane's WebContents is installed by
`ContentsWebView::SetWebContents(containers_[i]->contents_web_view_, wc)`. Two
guards stand between here and a 3rd pane: the literal `index < 2` CHECK, and the
bounds check against `containers_.count` (`[mcv+0x370]`), which the ctor sets to
**2** (`ctor.md` §4). The resize-area toggle is also 1-slot (`+0x3f0`, only on
`index==1`).

### After feeding: `MultiContentsView::ShowSplitView` (`0xc930ffc`) is N-safe

Driven separately via `BrowserView::MaybeUpdateSplitView` (`0x2b58058`, itself
called from `BrowserView::UpdateUIForContents+0xc4` `0x2b57f34`). Its body loops
the whole `containers_` vector setting each container's active flag
(`[container+0x38d] = (i==active)`) and writes the active index to `[mcv+0x428]`
— loop bound is `containers_.count`, so it needs no change for N (matches
`split-view-abi.md`).

---

## 4. The data model is already N-capable

### `split_tabs::SplitTabData::ListTabs` (`0x8184fe0`)

```
0x8184fe0: mov rcx,[rcx]                                ; this->collection_
0x8184fe3: jmp tabs::TabCollection::GetTabsRecursive    ; returns ALL tabs in the collection
```

A tail call. `ListTabs()` returns however many tabs the split's collection holds
— not a fixed 2.

### `tabs::TabStripCollection::CreateSplit` (`0x818834e`)

The function that actually makes a split. Its **only** cap is "more than one
tab":

```
0x818836d: cmp qword ptr [r8 + 8], 1                    ; r8 = tabs vector; [r8+8] = size
0x8188372: jbe <bail>                                   ; require size > 1  (NO upper bound)
...
0x81883f0: mov  ecx, 0x98 ; call operator new
0x8188417: call tabs::SplitTabCollection::SplitTabCollection   ; 0x8184e4a
0x8188440: call tabs::TabStripCollection::AddTabCollectionImpl
; then: loop i in 0..size:  MoveTabImpl(tabs[i] -> split collection)
0x818844d: shl rbx,3 (=size*8)
0x818845a: ... call tabs::TabStripCollection::MoveTabImpl
0x8188484: jne 0x18818845a                              ; moves EVERY passed tab in
```

It walks and moves **all** tabs in the argument vector into the new
`SplitTabCollection`. Pass it three tabs and you get a three-tab split; `ListTabs`
then returns three; `BrowserView::ShowSplitView`'s loop then calls index 2. The
model has no two-pane limit — the Edge UI simply never calls it with more than
two.

### Controller observer — `SplitWindowControllerImplV2::OnSplitTabAdded` (`0xa021d62`)

The controller's reaction to a split appearing. Note it grabs **first** and
**last** tab of the collection, not every tab:

```
0xa021ddc: call SplitTabData::ListTabs
0xa021df1: mov rax,[r12]; ... (vtable+0x18) call  ; rbx = tabs[0]->GetContents()        (FIRST)
0xa021e1b: call SplitTabData::ListTabs
0xa021e2e: mov rcx,[rsp+0x38]; mov rcx,[rcx+rax*8-8] ; r14 = tabs[count-1]->GetContents() (LAST)
0xa021eb9: call SetupHelpersForSplitWindowWebContents(this, FIRST, LAST)
0xa021f30: call SplitWindowControllerImplV2::ShowSplitView          ; 0xa01fa70
```

`SplitWindowControllerImplV2::ShowSplitView` (`0xa01fa70`) does the same
first/last extraction and then wires toolbar (`ToolbarSplitView`), input
listening and focus for those two sides. It does **not** call
`SetWebContentsAtIndex`; that is entirely `BrowserView`'s job (§3). So the
controller treats a split as exactly **two sides (first, last)** — a second
two-pane assumption, independent of the view's.

---

## 5. Feature gating — names, defaults, and whether they block the feature

`base::Feature` structs read via the `lea` targets of each gate (bytes at the
struct: `[+0]` = `const char* name`, `[+8]` = default state `0`=disabled /
`1`=enabled). Extracted with scratch `featread.py`.

| gate function | RVA | `base::Feature` symbol | feature name string | default |
|---|---|---|---|---|
| `IsEdgeSplitWindowEnabled` | `0xfb9f2d` | `kEdgeSplitWindow` `0x140f3ae8` | **`msEdgeSplitWindow`** | **1 (on)** |
| `IsEdgeSplitWindowMigrationToUpstreamEnabled` | `0xf975c0` | `kEdgeSplitWindowNewToolbar+0x28` `0x140f3bd8` | **`msEdgeSplitWindowTask57744098`** | **1 (on)** |
| `IsEdgeSplitWindowNewToolbarEnabled` | `0xf9754f` | `kEdgeSplitWindowNewToolbar` `0x140f3bb0` | `msEdgeSplitWindowNewToolbar` | 0 (off) |
| `IsEdgeSplitWindowDragAndDropEnabled` | `0x2b2296c` | `kEdgeSplitWindowDragAndDrop` `0x140f3b60` | `msEdgeSplitWindowDragAndDrop` | 0 (off) |
| (promotion) | — | `kEdgeSplitWindowShowPromotion` `0x140ea038` | `msEdgeSplitWindowShowPromotion` | 1 (on) |
| (converge ctx menu) | — | `kEdgeSplitWindowConvergeTabContextMenu` `0x140f3c28` | `msEdgeSplitWindowConvergeTabContextMenu` | 0 (off) |

`IsEdgeSplitWindowEnabled` (`0xfb9f2d`) is the master gate and is more than a
`base::Feature` read — it is `!IsXboxSku() && IsEnabledInLocalState() &&
FeatureList::IsEnabled(msEdgeSplitWindow)`, cached in a function-local static:

```
0xfb9f72: call base::edge::win::IsXboxSku ; jne <disabled>           ; Xbox -> off
0xfb9f7f: call edge::split_screen::IsEnabledInLocalState ; je <disabled>
0xfb9f8c: lea  rcx, kEdgeSplitWindow ; call base::FeatureList::IsEnabled
```

`IsEdgeSplitWindowMigrationToUpstreamEnabled` (`0xf975c0`) gates the whole V2 /
`MultiContentsView` architecture and requires `IsEdgeSplitWindowEnabled()` first,
then `FeatureList::IsEnabled(msEdgeSplitWindowTask57744098)`.

**Does a flag have to be flipped?** No. On 155.0.4283.45 both gates default
**enabled** (`msEdgeSplitWindow`=1, `msEdgeSplitWindowTask57744098`=1), so the V2
split UI is live out of the box — subject to the non-feature conditions
(`!IsXboxSku`, `IsEnabledInLocalState`, which reads local prefs/policy).
`msEdgeSplitWindowNewToolbar` is off by default and only changes whether
`ShowSplitView` builds the two-address-bar toolbar (`0xa01fb69`); it does not
gate splitting.

---

## 6. Adding a THIRD pane — what an injected DLL calls, and what blocks it

The control/data path needs **no patching** — it already loops. The work is all
in the view. To get a 3rd tab's WebContents into a 3rd pane:

1. **Make a 3-tab split (data side).** Call
   `tabs::TabStripCollection::CreateSplit` (`0x818834e`) with a 3-element tab
   vector (or move a third tab into an existing `SplitTabCollection`). Its
   `size > 1` guard (`0x818836d`) already allows this, and it moves every tab in.
   `SplitTabData::ListTabs` (`0x8184fe0`) will then return 3, so
   `BrowserView::ShowSplitView`'s loop (`0xc935b4b`) naturally reaches `i = 2`.

2. **Grow the view to 3 panes (`MultiContentsView`).** Before the loop reaches
   `i=2`, there must be a `containers_[2]`. Replicate the per-pane ctor block
   from `ctor.md` §4 (`EdgeContentsContainerView` ctor `0x2ce12b0`, its
   `ContentsWebView` at `+0x3a8`, parented via `AddChildViewAtImpl`,
   `emplace_back` into the `+0x368` vector) so `containers_.count` (`[mcv+0x370]`)
   becomes **3**.

3. **Defeat the `SetWebContentsAtIndex` cap.** The assignment itself
   (`0xc930f18`) refuses `index >= 2` at `0xc930f1f: cmp r8d,2 / jae ud2`. Either
   patch that compare (and the `[mcv+0x370]` bounds `jbe` at `0xc930f32`), or
   bypass the function and call
   `ContentsWebView::SetWebContents(containers_[2]->[+0x3a8], wc)` directly
   (`ContentsWebView::SetWebContents` `0x2b57520`).

4. **Add the 2nd resize area + N-pane geometry.** One more
   `EdgeMultiContentsResizeArea` (ctor `0x2ce11b0`) between panes 1 and 2, and
   replacements for `GetViewSizes` (`0x2b229e0`), `CalculateProposedLayout`
   (`0x2b22540`) and `CalculateSeparatorLayouts` (`0x2b22ae6`) that loop
   `containers_.count` instead of reading fixed slots `[0]`/`[1]` and emitting one
   separator (see `split-view-abi.md`; `GetViewSizes` is quoted there — it reads a
   2-entry size array indexed by `active∈{0,1}` and only
   `containers_[0]`/`containers_[1]`).

5. **(Optional) controller helpers for the middle pane.** The controller wires
   input/toolbar/focus for first+last only (`OnSplitTabAdded` `0xa021d62`,
   controller `ShowSplitView` `0xa01fa70`). A middle pane works visually without
   this, but to get its toolbar/focus/input parity you'd extend
   `SetupHelpersForSplitWindowWebContents` (`0xa022668`) to all tabs, not just
   first/last.

### The single biggest blocker

**The `MultiContentsView` is physically a two-pane widget, and its geometry is
hardwired to two.** The `index < 2` CHECK in `SetWebContentsAtIndex`
(`0xc930f1f`) is the loud symptom, but it is a two-byte patch; the real work is
that there is no `containers_[2]`, no second resize area, and the layout math
(`GetViewSizes` / `CalculateProposedLayout` / `CalculateSeparatorLayouts`) reads
fixed slots `[0]`/`[1]` and lays out exactly one separator. Everything *upstream*
of the view — `CreateSplit`, `ListTabs`, the `ShowSplitView` feed loop,
`MultiContentsView::ShowSplitView`/`ExecuteOnEachVisibleContentsView`, the active
index — already loops over `count`. So the splice point is the view's container
vector + resize areas + three geometry functions, not the controller or the tab
model.

---

## 7. Honest uncertainties

- **Who calls `CreateSplit` with how many tabs.** `CreateSplit` *accepts* N, but
  I did not trace every caller to prove nothing clamps the argument vector to 2
  before calling (the "add to split" command and the drag-drop path likely build
  a 2-element vector). The function body itself imposes no upper bound; the
  clamp, if any, is in its callers.
- **`SplitTabData`/`SplitTabCollection` invariants.** `ListTabs` →
  `GetTabsRecursive` returns all tabs, but there may be a `CHECK`/`DCHECK`
  elsewhere in the split-collection code (e.g. an `index_range` of width 2, or a
  "split has exactly 2 tabs" assertion) that fires in a non-shipping build but is
  compiled out here. Not audited.
- **The controller's first/last model.** `OnSplitTabAdded`/`ShowSplitView` assume
  two sides. A 3-pane split would leave the middle pane without controller-managed
  toolbar/input/focus. The visual split (via `BrowserView`) is independent of
  this, so panes would render, but UX parity for the middle pane is unverified.
- **`MaybeCreateController` arg semantics.** The arg-2 struct offsets
  (`+0x10`/`+0x30`) and the `[+8]==0` guard are read off the ctor; I inferred
  "browser-window features" from the caller
  (`EmbedderBrowserWindowFeatures::InitPostBrowserViewConstruction`) but did not
  fully type the struct.
- **Cold-section attribution.** Some call sites resolved to
  `Fn+0x1bcXXXXX` (e.g. `BrowserView::OnActiveTabChanged` → `ShowSplitView`).
  Those large displacements are MSVC hot/cold splitting (the cold half lives in a
  far code region with no own symbol); the attribution to the named function is
  correct, but the `+offset` is not a real intra-function offset.
```
