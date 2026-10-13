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
