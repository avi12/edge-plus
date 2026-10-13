# 2-D split geometry: tiling, the separator, and the resize-drag writeback

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`). Every `0x…` below is an **RVA** (linear =
`0x180000000 + rva`). Builds on `split-view-abi.md` and `ctor.md`.
Produced with `tools/re/disasm.py` + `scratchpad/drange.py`. Re-verify after an
Edge update.

> **Bottom line.** Edge lays the 2-pane split as a 1-D strip along one axis:
> `[pane0 | separator | pane1]`, widths `size0 + thickness + size1 = axis_len`.
> The real tiling lives in **`CalculateProposedLayout`** (not
> `CalculateSeparatorLayouts`, which only emits the ink-drop overlay and passes
> the content rect through). The split **ratio is model-owned**: a handle drag
> never stores to `MCV+0x430` directly — it walks
> `OnMouseDragged → ReportResizeAmount → MultiContentsView::OnResize →
> ResizeWebContents → TabStripModel::UpdateSplitRatio`, and the new ratio flows
> *back* into `MCV+0x430` via **`UpdateSplitVisualData`** on the next relayout.
> A handle's orientation is **not stored** — it is inferred at runtime from the
> handle's own bounds (`width < height ⇒ vertical bar`). All of this generalizes
> cleanly to N−1 handles: give each handle the right rect and everything else
> (cursor, drag axis) follows; drive each edge's ratio through our own tree store.

## Function map (RVA → symbol → role)

| RVA | symbol | role |
|---|---|---|
| `0x2b22540` | `MultiContentsView::CalculateProposedLayout` | **the tiler** — computes 3 rects, emits 5 ChildLayouts |
| `0x2b22ae6` | `MultiContentsView::CalculateSeparatorLayouts` | emits ink-drop overlay ChildLayout; returns content rect |
| `0x2b229e0` | `MultiContentsView::GetViewSizes` | `{size0, thickness, size1}` from ratio + thickness |
| `0x2b22aa2` | `MultiContentsView::ClampToMinSize` | per-pane minimum clamp (preserves sum) |
| `0xc9315e0` | `MultiContentsView::OnResize` | **delegate drag callback**; `this = MCV+0x338`; ratio = `(off+amount)/total` |
| `0xa4ffa00` | `MultiContentsViewDelegateImpl::ResizeWebContents` | pushes ratio into the model |
| `0xa272bae` | `TabStripModel::UpdateSplitRatio` | **ratio's authoritative home** (model) |
| `0xc9310ec` | `MultiContentsView::UpdateSplitVisualData` | **the only store to `MCV+0x428/0x430`** (16-byte copy from model) |
| `0xa01606c` | `EdgeMultiContentsResizeArea::SetLayout` | sets the handle's preferred thickness from orientation |
| `0xb1facb0` | `views::EdgeTwoDimensionalResizeArea::OnMouseDragged` | reads event x/y, calls `ReportMouseResizeAmount` |
| `0xb1fab88` | `…::ReportMouseResizeAmount` | picks the axis delta |
| `0xb1faeb0` | `…::ReportResizeAmount` | RTL flip, then `delegate->OnResize(amount, done)` via `area+0x348` |
| `0xb1faaa0` | `…::OnGestureEvent` | touch/gesture equivalent of the drag |
| `0xb1faa8e` | `…::IsVerticalResizeView` | **orientation from own bounds**: `[+0x1d0] < [+0x1d4]` |
| `0xb1faa40` | `…::GetCursor` | same bounds test → E-W (`0xf`) vs N-S (`0xe`) cursor |
| `0x2cd1366` | `views::EdgeTwoDimensionalResizeArea::EdgeTwoDimensionalResizeArea` | stores delegate at `area+0x348` |
| `0x278099a` | `emplace_back<View*, bool, const gfx::Rect&>` | **the append primitive to reuse** |
| `0x29a34bc` | `emplace_back<EdgeMultiContentsResizeArea*, bool, gfx::Rect&>` | same thing, separator overload |
| `0x2780694` | `emplace_back<InkDropContainerView*, bool, gfx::Rect>` | same thing, ink-drop overload |

### Delegate subobject offsets (`MultiContentsView::OnResize` `this = MCV+0x338`)

`OnResize` is a virtual on the resize-delegate subobject embedded at `MCV+0x338`
(the one the resize area stores at `area+0x348` and dispatches through). Field
map (all confirmed by the `+0x338` adjustment):

| in OnResize | = MCV offset | meaning |
|---|---|---|
| `[this+0x20]` | `MCV+0x358` | `MultiContentsViewDelegateImpl*` (gets `ResizeWebContents`) |
| `[this+0x30]` | `MCV+0x368` | containers vector begin |
| `[this+0x38]` | `MCV+0x370` | pane count |
| `[this+0xf0]` | `MCV+0x428` | orientation (`SplitTabLayout`) |
| `[this+0xf8]` | `MCV+0x430` | ratio (double) — *not written here* |
| `[this+0x11c]` | `MCV+0x454` | drag-start offset along axis |
| `[this+0x120]` | `MCV+0x458` | "resize in progress" latch byte |
| `[this+0x140/0x148]` | `MCV+0x478 / +0x480` | begin/count of **snap-ratio** candidates (double[]) |

---

## 1. `CalculateProposedLayout` — the real tiler (`0x2b22540`)

`this = MCV+0x340` (the `LayoutManagerBase` subobject); it recovers
`r14 = MCV = this-0x340` at `0x2b22594`. `rdx(rsi)` = out `ProposedLayout*`,
`r8(rbx)` = in available-size `{int w @+0, bool @+4, int h @+8, bool @+12}`.

Field access through `this`: `[this+0x28]=MCV+0x368` (containers),
`[this+0x30]=MCV+0x370` (count), `[this+0x68]=MCV+0x3a8` (full-host overlay),
`[this+0x70]=MCV+0x3b0` (ink-drop container), `[this+0xb0]=MCV+0x3f0` (resize
area), `[this+0xe8]=MCV+0x428` (orientation).

### Flow

1. `ProposedLayout::ProposedLayout(out)` zeroes it (`0x2b2257b`).
2. Require both input validity bytes (`[rbx+4]==1 && [rbx+0xc]==1`), else jump to
   the tail and return an empty layout (`0x2b22580`, `0x2b2258a`).
3. Build the **content rect** `= {0, 0, host_w, host_h}` into `[rsp+0x90]`,
   where `host_w = max(0,[rbx])`, `host_h = max(0,[rbx+8])` (`0x2b2259b`…`0x2b225c2`).
4. `CalculateSeparatorLayouts(MCV, &sep@rsp+0x80, &content@rsp+0x90, &out.child_layouts)`
   (`0x2b225f7`) — emits the ink-drop overlay ChildLayout and (if a drop target
   is active) insets the content rect; the returned rect is copied back into
   `[rsp+0x90]` (`0x2b225fc`).
5. `GetViewSizes(MCV, &result@rsp+0x70, &content@rsp+0x90)` (`0x2b22643`) →
   `result = {size0, thickness, size1}`.
6. A long `base::Clamp{Add,Sub}` block (`0x2b22648`…`0x2b22749`, orientation==0
   path; `0x471bce8` cold chunk for orientation!=0) lays three abutting rects and
   stores them:

   | stack | view | rect (orientation **0 = columns**, split along **x**) |
   |---|---|---|
   | `[rsp+0x80]` | `container[0]` (`MCV+0x368[0]`) | `{content.x, content.y, size0, host_h}` |
   | `[rsp+0x50]` | separator (`MCV+0x3f0`) | `{content.x+size0, content.y, thickness, host_h}` |
   | `[rsp+0x40]` | `container[1]` (`MCV+0x368[1]`) | `{content.x+size0+thickness, content.y, size1, host_h}` |

   For orientation **≠ 0 = rows** (cold chunk `0x471bce8`) the same three rects
   are stacked along **y**: widths become `host_w`, and `size0 / thickness /
   size1` are consumed as **heights** at increasing `y`. So the only difference
   between the two orientations is which axis carries `{size0, thickness, size1}`.

7. **Emit order** (each is a `ChildLayout{View*, bool visible = child->GetVisible(),
   gfx::Rect}` appended to `out.child_layouts`):

   | # | RVA | child | rect | emplace overload |
   |---|---|---|---|---|
   | 1 | `0x2b227b4` | ink-drop container `MCV+0x3b0` | sep rect `[rsp+0x90]` | `InkDropContainerView*` (`0x2780694`) |
   | 2 | `0x2b22804` | `container[0]` | `[rsp+0x80]` | `View*` (`0x278099a`) |
   | 3 | `0x2b22834` | separator `MCV+0x3f0` | `[rsp+0x50]` | `EdgeMultiContentsResizeArea*` (`0x29a34bc`) |
   | 4 | `0x2b22883` | `container[1]` | `[rsp+0x40]` | `View*` (`0x278099a`) |
   | 5 | `0x2b228bc` | full-host overlay `MCV+0x3a8` | `{0,0,host_w,host_h}` | `View*` (`0x18109c87c`) |

   `ApplyLayout` names each View explicitly, so **emit order is irrelevant** to
   correctness. All overloads are the same append; a detour can use `0x278099a`
   for every child (a container and a resize area are both `views::View*`).

### Generalized separator placement

A separator at boundary offset `b` (from the content origin, along the split
axis) with thickness `t`:

```
columns (orientation 0): sep = { content.x + b, content.y, t, content.h }
rows    (orientation 1): sep = { content.x, content.y + b, content.w, t }
```

Edge's single case is `b = size0`, panes abut with the strip carved out:
`pane0 = [0, size0)`, `sep = [size0, size0+t)`, `pane1 = [size0+t, axis_len)`,
and `size0 + t + size1 = axis_len`. Reserve one `t` per internal boundary.

---

## 2. `CalculateSeparatorLayouts` (`0x2b22ae6`) — not the separator placer

Signature `(rcx=MCV, rdx=out gfx::Rect, r8=in gfx::Rect, r9=&child_layouts)`.
It does **not** read `+0x428` or `+0x430`. It:

- Gates on `MCV+0x3f0`'s `GetVisible()` (`0x2b22b18`).
- **Resize area visible** (normal split, cold chunk `0x471bef1`): emits the
  ink-drop (`MCV+0x380`) ChildLayout with `visible=false` and rect `{0,0,0,0}`,
  then `*out = *in` — i.e. passes the content rect straight through.
- **Not visible** (fall-through `0x2b22b25`): computes the ink-drop's own rect
  (insetting by its `GetPreferredSize().height` when the bool at `MCV+0x3a0` is
  set, via cold chunk `0x471becb`) and emits it; the returned rect is the content
  area minus that inset.

So the only "separator math" here is the DnD/ink-drop overlay. **The separator's
real position/size comes from `CalculateProposedLayout` step 6**, driven by
`GetViewSizes`. For an N-pane detour this function can be left to run as-is (it
only touches the single ink-drop overlay).

---

## 3. `GetViewSizes` (`0x2b229e0`) + `ClampToMinSize` (`0x2b22aa2`)

`GetViewSizes(rcx=MCV, rdx=out, r8=content rect)`. `out` is a 3-int struct
`{size0 @+0, thickness @+4, size1 @+8}`.

- `orient = (MCV+0x428 != 0)` (`0x2b22a01`). `axis_len = content[8 + orient*4]`
  (width for columns, height for rows) into `ebp` (`0x2b22a0e`).
- Gate on `MCV+0x3f0` visible. The **ratio-split** math is the cold chunk
  `0x471bf3a`:

  ```
  require container[0].GetVisible && container[1].GetVisible
  thickness = resize_area.GetPreferredSize().{width if orient==0 else height}   ; = 8 by default
  avail     = axis_len - thickness
  size0     = (int)round( (double)avail * *(double*)(MCV+0x430) )                ; ratio
  size1     = avail - size0
  ```
  (`0x471bfba`…`0x471bfdd`.) Default when the handle is hidden: `size0=axis_len,
  thickness=0, size1=0`.
- Then `ClampToMinSize(MCV, out, &content_copy, &{size0,thickness,size1})`.

**Separator thickness** = `resize_area->GetPreferredSize()`. Ctor sets it to
`{8,8}` (`ctor.md`), and `SetLayout` (§5) rewrites it to `{8,0}` (columns) or
`{0,8}` (rows). Net: **8 px**.

### `ClampToMinSize` — per-pane minimum (cold chunk `0x471c3d1`)

```
len   = (orient==0) ? content.w : content.h
cand  = max( 50, (int)(len * 0.1) )                    ; 0.1 @ rva 0x118d98e8, 50 = 0x32
cap   = (*(byte*)(MCV+0x474) != 0) ? *(int*)(MCV+0x470) : <rodata default>
min   = min( cand, cap )
if size0 < min: size0 = min; size1 -= (min - size0_old)   ; sum preserved
elif size1 < min: size1 = min; size0 -= (min - size1_old)
```
**Min-size source:** `std::optional<int>`-like pair at `MCV+0x470` (value) /
`MCV+0x474` (has-value byte); when unset, the rodata default read back as `0`
(so the floor collapses to 0 unless the optional is populated). The live floor is
effectively `max(50 px, 10% of the axis length)`, capped by the optional. Only
two panes are clamped (size0, size1).

> For our tree we only need to match the spirit: clamp each leaf to
> `max(50, 0.1 * axis_len_of_its_parent_split)` and push the slack onto the
> dragged neighbor, so the handle can't collapse a pane below Edge's floor.

---

## 4. The resize-drag writeback — ratio is **model-owned**

### The call chain

```
EdgeTwoDimensionalResizeArea::OnMouseDragged   0xb1facb0
   reads event.x @[evt+0xb0], event.y @[evt+0xb4]
   → ReportMouseResizeAmount(this, x, y, 0)     0xb1fab88       (picks axis)
      → ReportResizeAmount(this, amount, done)  0xb1faeb0
           delegate = *(void**)(this + 0x348)   ; = MCV+0x338
           if (IsRTL && width<height) amount = -amount
           → delegate->vtbl[0](delegate, amount, done)          ; virtual
                = MultiContentsView::OnResize    0xc9315e0       (this = MCV+0x338)
                     total  = size(container0)+size(separator)+size(container1) along axis
                     ratio' = (double)(drag_start_off + amount) / (double)total
                     ratio' = snap(ratio', MCV+0x478[])          ; snap to candidates within eps
                     → ResizeWebContents(*(MCV+0x358), ratio', done)   0xa4ffa00
                          → TabStripModel::UpdateSplitRatio(tab, split_id, ratio', done)  0xa272bae
                          → SessionService::SetSplitTabData(...)        ; persist
```

- The handle stores its delegate at **`area+0x348`** (set in the
  `EdgeTwoDimensionalResizeArea` ctor at `0x2cd1411`, `mov [rsi+0x348], rdi`)
  and dispatches `OnResize` through it (`ReportResizeAmount` `0xb1faee4`).
- `OnResize` (`0xc9315e0`) computes the new ratio as
  `(drag_start_offset + amount) / total_axis_length` (`0xc93170c`…`0xc93174d`)
  and **optionally snaps** it to one of the candidate ratios in the vector at
  `MCV+0x478 / +0x480` (loop `0xc931796`; `|cand*total - (off+amount)| < eps`).
- It then calls `ResizeWebContents` which calls
  **`TabStripModel::UpdateSplitRatio`** — i.e. the ratio's single source of truth
  is the **`SplitTabData` / `SplitTabVisualData` in the tab-strip model**, not the
  view. No store to `MCV+0x430` happens on the drag path.

### Where `MCV+0x430` actually gets written

`MultiContentsView::UpdateSplitVisualData(rcx=MCV, rdx=SplitTabVisualData*)`
(`0xc9310ec`), called from `ShowSplitView`:

```
if ([MCV+0x428] == newData->layout) && ([MCV+0x430] == newData->ratio) return;   ; no-op guard
if (layout changed) for each container[i]: container[i][+0x38d] = (newData->layout == 1)
movups xmm0, [rdx] ; movups [MCV+0x428], xmm0     ; <-- THE store: 16-byte copy of
                                                  ;     SplitTabVisualData{int layout @0, double ratio @8}
EdgeMultiContentsResizeArea::SetLayout(MCV+0x3f0, newData->layout)               ; 0xa01606c
InvalidateLayout()
```

So `MCV+0x428` (orientation) and `MCV+0x430` (ratio) are a single
`split_tabs::SplitTabVisualData` **cached copy**, overwritten wholesale from the
model (`0xc931156`). No clamp in this store — the clamp/snap already happened in
`OnResize`, and the layout re-derives pane sizes from the fresh ratio via
`GetViewSizes`.

> **Consequence for N panes:** per-edge ratios cannot live in the view's single
> `+0x430` slot. Each internal tree boundary needs its own ratio, stored in **our
> own tree store** (or, if we want Edge's session persistence, N−1 entries in the
> model). A handle drag must map `handle → tree edge → that edge's ratio`, run the
> same `(off+amount)/total_of_that_edge` formula, clamp to the per-leaf minimum,
> and write that edge's ratio; then the detoured layout re-tiles from the tree.

---

## 5. Resize-area construction for N−1 handles & orientation

**Construction recipe** (unchanged from `ctor.md`), per gap:

```
p = HandleMemorySafetyCheckedOperatorNew<3>(0x358)          ; 0x101bd60
EdgeMultiContentsResizeArea::EdgeMultiContentsResizeArea(p, mcv)   ; 0x2ce11b0, rdx = mcv
AddChildViewAtImpl(mcv, p, *(size_t*)(mcv+0x1a8))           ; 0x1017902  (append)
store p into our own area[] slot                           ; NOT the single mcv+0x3f0
```

The ctor passes `mcv` to `EdgeTwoDimensionalResizeArea` (`0x2ce11b0`:
`lea rax,[rdx+0x338] ; cmovne rdx,rax`), which stores the delegate `mcv+0x338` at
`p+0x348` (`0x2cd1411`). **Every handle shares the same delegate `MCV+0x338`** —
that is fine as long as `OnResize` can tell which edge fired. Edge's single
handle never needs to; our detour must disambiguate (give each handle a distinct
`SetID`/`SetProperty`, or replace `OnResize` and look up the handle by sender).

**Orientation is not a per-area field.** Two independent mechanisms, both derived
from geometry:

1. **Preferred thickness** via `EdgeMultiContentsResizeArea::SetLayout(area,
   orientation)` (`0xa01606c`): `orientation!=0 → SetPreferredSize({0,8})`
   (horizontal bar), else `({8,0})` (vertical bar). Called from
   `UpdateSplitVisualData`. Only matters if Edge's own layout sizes the handle;
   our detour sets the rect directly and can skip it.
2. **Drag axis & cursor at runtime** from the handle's **own bounds**:
   `IsVerticalResizeView` (`0xb1faa8e`) and `GetCursor` (`0xb1faa40`) both do
   `width([+0x1d0]) < height([+0x1d4])` → vertical divider (E-W drag, cursor
   `0xf`), else horizontal divider (N-S drag, cursor `0xe`).

> **So per-handle orientation is automatic**: assign a handle a tall-thin rect and
> it becomes a vertical (column) divider; a wide-short rect makes it a horizontal
> (row) divider. No per-area flag, no reading `MCV+0x428` at drag time.

---

## 6. Recommendation — the N-pane geometry + resize detour

**Layout (detour `CalculateProposedLayout`, `0x2b22540`).** When our per-MCV tree
has >2 leaves, after `ProposedLayout::ProposedLayout` zeroes the out:

1. Build `content = {0,0,host_w,host_h}` exactly as Edge does (step 3), optionally
   letting the stock `CalculateSeparatorLayouts` run for the ink-drop overlay.
2. Walk the split tree to N leaf rects, reserving **8 px** (`thickness`) at every
   internal boundary and dividing each split node's axis span by that node's ratio
   with the same `round(avail*ratio)` rounding and the
   `max(50, 0.1*axis_len)` per-leaf min clamp (§3) so we don't fight Edge's math.
3. `emplace_back(&out.child_layouts, &view, &visible, &rect)` via **`0x278099a`**
   once per leaf container (`View*`) and once per internal boundary's resize area
   (also a `View*`). This is the only allocator-safe append.
4. Give each handle a rect that is 8 px along its split axis and spans the shared
   edge on the cross axis — orientation then follows from the aspect (§5). No
   `SetLayout` needed when we set the rect ourselves.

**Handles.** Allocate **N−1** `EdgeMultiContentsResizeArea`s with the §5 recipe,
each parented under the MCV, stored in our own `area[]` (the stock single
`MCV+0x3f0` slot stays for the degenerate 2-pane case or is repurposed as
`area[0]`). Tag each with its tree-edge id (`SetID`/property) at creation.

**Drag → ratio.** Detour `MultiContentsView::OnResize` (`0xc9315e0`), or intercept
`ReportResizeAmount` (`0xb1faeb0`) which passes the sender handle: map the sender
→ its tree edge, compute `ratio' = clamp((edge_offset + amount) / edge_axis_len)`
with the §3 floor, write it to **that edge's ratio in our tree store**, and
`InvalidateLayout()`. Do **not** route through `TabStripModel::UpdateSplitRatio`
for interior edges (it only models one ratio); keep the model call only for the
root split if we want Edge's session persistence. Each handle therefore needs its
**own ratio storage** — the view's single `+0x430` is only the root/legacy slot.

**Why this is low-risk.** The view already treats the ratio as a cache refreshed
from an external source (`UpdateSplitVisualData`), so replacing that source with
our tree store is in-grain with Edge's own design; the handle's orientation and
cursor are self-describing from bounds; and `emplace_back` + `AddChildViewAtImpl`
are the exact primitives the stock ctor and layout already use.
