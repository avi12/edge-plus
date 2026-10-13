# Edge split-view ABI — `MultiContentsView` and friends

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`), via `tools/re/symbols.py` + `tools/re/disasm.py`.
Every RVA and offset below is pinned to this build; re-verify after an Edge update.

> **The point.** Edge's native "Split Screen" caps at two panes. The *storage*
> is already a `std::vector<ContentsContainerView*>` and several methods loop it
> generically — the cap lives only in (a) the geometry math, (b) one `index < 2`
> guard, and (c) a single resize-area slot. Those are the splice points for N panes.

## Key functions (RVA → symbol)

| RVA | symbol | role |
|---|---|---|
| `0x2cdf896` | `MultiContentsView::MultiContentsView` | ctor — builds the 2 containers + resize area *(ctor.md)* |
| `0xc930f18` | `MultiContentsView::SetWebContentsAtIndex` | **hard cap `index < 2`**; puts a WebContents into container[i] |
| `0xc930ffc` | `MultiContentsView::ShowSplitView` | loops the whole vector; sets active flags + `+0x428` |
| `0x2b22540` | `MultiContentsView::CalculateProposedLayout` | **2-entry** layout + 1 separator |
| `0x2b229e0` | `MultiContentsView::GetViewSizes` | **2-element** size array; reads container[0],[1] |
| `0x2b22ae6` | `MultiContentsView::CalculateSeparatorLayouts` | lays the single separator |
| `0x2b22aa2` | `MultiContentsView::ClampToMinSize` | clamps to per-pane minimum |
| `0x2b56844` | `MultiContentsView::ExecuteOnEachVisibleContentsView` | **N-ready** — loops `count` elements |
| `0x2cd0f00` | `edge::split_screen::EdgeMultiContentsResizeArea::SetVisible` | the resize handle |
| `0x2ccfac2` | `edge::split_screen::SplitWindowControllerImplV2::SplitWindowControllerImplV2` | controller ctor *(controller.md)* |
| `0x2ccea6a` | `edge::split_screen::SplitWindowController::MaybeCreateController` | split entry point *(controller.md)* |
| `0xfb9f2d` | `features::edge::IsEdgeSplitWindowEnabled` | feature-flag gate |

## Struct layout (verified from disassembly)

### Layout entry point — the multiple-inheritance adjustment (verified 2026-10-13)

`MultiContentsView : public views::View, public views::LayoutManagerBase`. The
`views::View` base is at `+0`; the **`LayoutManagerBase` subobject is embedded at
`+0x340`**. `CalculateProposedLayout` (rva `0x2b22540`) is the *LayoutManagerBase*
virtual override, so **its `this` is the subobject at `MCV+0x340`, not the MCV**.
It immediately does `lea r14,[this-0x340]` to recover the MCV, then calls the
MCV-relative helpers with that:

- `GetViewSizes(rcx = MCV)` — reads count `[MCV+0x370]`, data `[MCV+0x368]`,
  resize area `[MCV+0x3f0]`, orientation `[MCV+0x428]`. (So these helpers take the
  **MCV**; only `CalculateProposedLayout` takes the subobject.)
- Inside `CalculateProposedLayout`, `[this+0xb0]` = `MCV+0x3f0` (resize area,
  `GetVisible`-checked) and `[this+0xe8]` = `MCV+0x428` (orientation). Earlier these
  were mis-read as MCV `+0xb0`/`+0xe8`; they are subobject-relative.

**Hook consequence:** a detour on `CalculateProposedLayout` receives
`this = MCV + 0x340`; recover `mcv = this - 0x340` before touching any MCV field.
`rdx` = out `views::ProposedLayout*` (its ctor zeroes `0x20` bytes: `gfx::Size
host_size` at `+0`, `std::vector<ChildLayout> child_layouts` at `+8`), `r8` = a
2-entry `{int size @+0, bool visible @+4}` array (stride 8).

### `MultiContentsView` (a `views::View` + `views::LayoutManagerBase` subclass)
| offset | type | meaning | evidence |
|---|---|---|---|
| `+0x368` | `ContentsContainerView** ` | vector data begin | `[this+0x368][i*8]` in GetViewSizes/ShowSplitView/SetWebContentsAtIndex |
| `+0x370` | `size_t` | pane count | `cmp [this+0x370],1` guards; loop bound in ExecuteOnEach |
| `+0x3f0` | `EdgeMultiContentsResizeArea*` | the one resize handle (single slot) | SetWebContentsAtIndex, GetViewSizes |
| `+0x428` | `split_tabs::SplitTabLayout` (int) | **orientation** enum, not active index | `SplitTabVisualData` ctor `[rcx]=layout`; GetViewSizes `setne al` picks width/height |
| `+0x430` | `double` | split ratio (default `0.5`) | `SplitTabVisualData` ctor writes `0x3fe0…`=0.5 at `+8` of the struct at `+0x428` |
| `+0xb0`  | `views::View*` | drop-target view (checked for visibility) | CalculateProposedLayout |
| `+0xe8`  | `int` | a mode flag (RTL or DnD) | CalculateProposedLayout |
| `+0x350` | `BrowserView*` | passed to each container ctor | ctor.md |
| `+0x1a8` | `size_t` | child-view insert index | ctor.md (`AddChildViewAtImpl`) |

> **Correction:** `+0x428` was first read as "active index"; it is the
> `SplitTabVisualData` the ctor builds there — field 0 is the **orientation**
> (`SplitTabLayout`), field `+8` the split ratio. The active pane is tracked on
> `ContentsWebView+0x450` (ctor.md), not here.

### `ContentsContainerView` / `EdgeContentsContainerView` (0x558 bytes)
| offset | type | meaning | evidence |
|---|---|---|---|
| `+0x3a8` | `ContentsWebView*` | the pane's web view | SetWebContentsAtIndex → `ContentsWebView::SetWebContents` |
| `+0x38d` | `bool` | orientation flag (set from `+0x428`) | ShowSplitView sets `(layout==1)` on every container |

`ContentsWebView+0x450` = active bool. Pane object = `EdgeContentsContainerView`,
`new` at rva `0x101bd60` (size `0x558`), ctor rva `0x2ce12b0` (builds the whole
child chain incl. the `ContentsWebView`). See `ctor.md`.

## `ProposedLayout` / `ChildLayout` ABI (verified 2026-10-13)

From `views::LayoutManagerBase::ApplyLayout` (rva `0xfd3ed8`) and the emit path of
`CalculateProposedLayout`:

- **`views::ProposedLayout`** (ctor `0xfbee34` zeroes `0x20` bytes):
  `+0x00 gfx::Size host_size`, `+0x08 std::__Cr::vector<ChildLayout> child_layouts`
  ({begin,end,cap} libc++ pointers).
- **`views::ChildLayout`** — **stride `0x30`**: `+0x00 View* child`, `+0x08 bool
  visible`, `+0x0c gfx::Rect bounds {x,y,w,h ints}`, `+0x1c SizeBounds
  available_size` ({SizeBound width_{int,bool}, SizeBound height_{int,bool}}).
  `ApplyLayout` loops `begin..begin+count*0x30`, calling `View::GetVisible`,
  `SetVisible`, and `SetBoundsRect(&bounds)` per entry.
- **Append primitive:** `std::vector<ChildLayout>::emplace_back<View*, bool, const
  gfx::Rect&>` at rva `0x278099a` — `rcx=&vector, rdx=&View*, r8=&bool,
  r9=&gfx::Rect`. The 2-pane emit calls it once per child (ink-drop container at
  `MCV+0x3b0`, container[0] at `MCV+0x368`, resize area at `MCV+0x3f0`,
  container[1], …). **Calling this is the only allocator-safe way to add entries
  from the injected DLL** — never hand-roll the vector, or the ProposedLayout dtor
  frees a pointer we allocated with the wrong allocator.

**N-pane geometry plan (revised):** in a `CalculateProposedLayout` detour, when
count ≤ 2 defer to the original (zero regression); when count > 2, build the
`host_size`, then `emplace_back` one entry per container with the N-way rect and
one per separator, computing rects by splitting the content length by ratio.

## Layout-agnostic by construction

`CalculateProposedLayout` is handed the `MultiContentsView`'s **own** available
size (`SizeBounds` arg), which `BrowserViewLayout` has already reduced for
whatever surrounds the split — vertical tab strip (takes width), toolbar /
omnibox row / bookmarks bar (takes height), side panels, etc. The detour must
therefore use **only** that size and position panes in **MCV-local coordinates**
(`{0,0,width,height}`), never window/screen coordinates. It does: `geometry::
TilePanes` tiles the tree within the handed rectangle, so vertical tabs, an
omnibox moved into the toolbar, a different tab layout, or a window resize all
just change that rectangle and the panes reflow into it — one mechanism, no
special cases. Keep it this way: no absolute coordinates in the geometry path.

## MILESTONE (2026-10-13): N-pane render proven live

A throwaway Edge, DLL injected with `FIXEDGE_PANES=3`: a normal 2-tab split was
grown to **3 containers** and the `CalculateProposedLayout` detour tiled **3 panes
(1889×2004, cols)** across repeated relayouts and user interaction with **no
crash**. The view-side cap is broken. The 3rd pane is blank (no WebContents wired
to index 2 yet). Evidence: `%LOCALAPPDATA%\FixEdge\split-native.log`.

## Target UX (Arc-style) and the revised model

The flat equal-columns model (`FIXEDGE_PANES`) is a proof, not the product. The
product is a **binary split tree**: each pane carries a 3-dot menu with **add
pane above / below / left / right**; each action splits that pane's tree node, so
the layout is an arbitrary grid. The native pieces this maps onto:

- Native already: the split button (unsplit state) → two panes + "pick a tab"
  surface; the ✕ → unsplit to full width; choosing a tab → fills the new pane.
- **Split button while already split = collapse:** keep only the **active** pane,
  resize it to fill the **whole tab window (full width and full height)**, and
  **eject every other pane as its own
  standalone tab** (remove them from the `SplitTabCollection` so they return as
  normal tabs — not closed). This is the toggle's "off" direction.
- **`SplitTabMenuModel`** (ctor `0xa264f8e`, `ExecuteCommand` `0xa265830`,
  `GetLabelForCommandId` `0xa265430`) is the existing 3-dot menu — the extension
  point for the four directional "add pane" commands. `SplitTabSwapMenuModel`
  and `SplitViewLayoutMenuModel` are its sibling submodels.
- Geometry: the proven `emplace_back`-per-container detour already positions a
  container at an arbitrary `gfx::Rect`, so a recursive tree→rects walk drops
  straight into it (each leaf = one container).

### Pane operations (Avi's spec, continued)

- **Swap button with a direction algorithm.** Each pane gets a swap toggle whose
  **axis is decided by the pane's place in the grid**: a top pane swaps only with
  its bottom counterpart (and vice-versa); a 3×4-style layout swaps horizontally.
  Interpretation to confirm: in the split **tree**, a leaf's swap counterpart is
  its **sibling subtree**, and the offered direction is the **parent node's split
  orientation** (vertical parent → top/bottom toggle, horizontal parent →
  left/right). This generalizes the 4×3 / 3×4 examples to any grid. Maps onto the
  native `SplitTabSwapMenuModel` (rva `0xa265ebe`) / `SplitViewLayoutMenuModel`.
- **A third handle** under the 3-dot button to **drag a pane to another slot**,
  pushing the others out of the way. Match the *feel* of `C:\repositories\avi\pade`
  (`src/lib/drag-reorder.ts` + `reorder.ts`), adapted from its flat left/right row
  to our 2D tree:
  - **Lift**: drag begins past a ~4px threshold; the lifted pane follows the
    pointer, raised (shadow), the rest get a `translate` transition.
  - **Reflow (push)**: snapshot every sibling's pre-drag rect once, then set each
    one's `translate` to the delta between its old slot and the slot it now
    occupies — an **insert-with-reflow, never a swap** (pade `reflow()`
    drag-reorder.ts:298; `reorderedIds` reorder.ts:20).
  - **Drop target**: pointer-vs-midpoint. Pade has 2 zones (`paneDropSide`
    reorder.ts:78); we need **4** — top/bottom/left/right halves of the hovered
    pane — so a drop can split along either axis, matching the tree.
  - **Settle**: on release, spring the lifted pane from the release point into the
    open slot, then commit the model after it settles (`settleIntoSlot`
    drag-reorder.ts:617). Spring ease `cubic-bezier(0.34,1.4,0.5,1)`, ~300ms;
    siblings `~240ms`. Animate **translate only** (never width/height) — natively,
    interpolate each container's `gfx::Rect` per frame toward its target.
- **Animated transitions** for every pane switch/move — but only when Windows
  animations are enabled: gate on `SystemParametersInfo(SPI_GETCLIENTAREAANIMATION)`
  (pade's equivalent gate is `prefers-reduced-motion`, drag-reorder.ts:161).
  Animate by interpolating each container's `gfx::Rect` across frames in the
  layout detour, translate-only, M3 spring easing — pade's FLIP, done natively.

Remaining build: a per-MCV split **tree** in the DLL (leaves ↔ containers),
tree→rect layout in the detour, extend `SplitTabMenuModel` with 4 directional
commands that insert a leaf + open the tab picker, and N−1 resize handles along
tree edges.

## The cap, precisely

- **`SetWebContentsAtIndex(index, wc)`** starts `cmp r8d, 2 / jae ud2` — a
  `DCHECK(index < 2)`. Only `index==1` touches the resize area (`+0x3f0`).
- **`GetViewSizes`** fills a two-`int` array (`r8[active*4+8]`) and only reads
  `container[0]` and `container[1]` (`[data+8]`).
- **`CalculateProposedLayout`** requires two `{int size, bool visible}` entries
  (`[r8+0],[r8+8]` sizes; `[r8+4],[r8+0xc]` flags) and calls
  `CalculateSeparatorLayouts` once for one separator.

## What is already N-ready (no change needed)

The controller agent (`controller.md`) proved the whole path **above** the view is
generic over pane count:

- `tabs::TabStripCollection::CreateSplit` (rva `0x818834e`) guards only `size > 1`
  and moves **all** passed tabs into one `SplitTabCollection`.
- `SplitTabData::ListTabs` (rva `0x8184fe0`) returns however many tabs it holds.
- The **only** feeder of panes, `BrowserView::ShowSplitView` (rva `0xc93594a`),
  loops `ListTabs()` at rva `0xc935b4b` calling
  `SetWebContentsAtIndex(mcv, tab[i]->GetContents(), i)` — already passes `i=2…`.
- `MultiContentsView::ShowSplitView`, `ExecuteOnEachVisibleContentsView`, and the
  active-index path already loop over `count` (`+0x370`).

**Feature flags are default-on** on this build and need no flipping:
`msEdgeSplitWindow` (gate, rva `0xfb9f2d`) and `msEdgeSplitWindowTask57744098`
(the V2/upstream architecture). (Also requires `!IsXboxSku && IsEnabledInLocalState`.)

So a 3-tab split already *tries* to feed pane 2 — it just hits the view's cap.

## N-pane splice plan (what a native DLL replaces/augments)

1. **Containers** — grow the `+0x368/+0x370` vector to N `ContentsContainerView`s,
   each built like the ctor does (→ `ctor.md`), parented under the view.
2. **Resize areas** — the single `+0x3f0` handle becomes N−1 handles the layout
   positions between neighbours.
3. **Geometry** — replace `GetViewSizes`, `CalculateProposedLayout`,
   `CalculateSeparatorLayouts` with versions that loop `count`, split the length
   by per-pane ratios, and emit N−1 separators.
4. **Insert** — an N-capable analogue of `SetWebContentsAtIndex` (drop the
   `< 2` guard, address the right resize area), fed by the controller pipeline
   that currently wires two tabs' WebContents (→ `controller.md`).

Iteration (`ExecuteOnEachVisibleContentsView`, `ShowSplitView`) and the active
index are already N-safe and need no change.
