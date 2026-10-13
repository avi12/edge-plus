# How the split's child views are built — the "create one more pane" recipe

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`). Every `0x2…`/`0x1…` number below is an **RVA**
(file offset in the image = RVA; `base+rva` is the linear address capstone
prints, e.g. RVA `0x2ce12b0` → `0x182ce12b0`). Re-verify after an Edge update.

Evidence was produced with `tools/re/symbols.py` and a full-length capstone
disassembler (`scratchpad/dis.py`, same resolver as `tools/re/disasm.py` but
without the 500-byte cap). Builds on `split-view-abi.md`.

> **Bottom line.** Each pane is one **`EdgeContentsContainerView`** (the concrete
> type — *not* the plain `ContentsContainerView`, which only ever exists as its
> base subobject). The ctor builds exactly **two** of them with an identical
> 5-step block, and drops **one** `EdgeMultiContentsResizeArea` between them.
> The block is already index-agnostic except for two post-insert fixups and the
> single resize-area slot — so pane *i* is a copy-paste of the block with `i`
> substituted. An injected DLL replicates that block N times (and adds N−1
> resize areas); it never has to touch a vtable, because it **calls the real
> ctors**, which install the vtables themselves.

---

## 1. Signatures and the allocator convention

| what | RVA | notes |
|---|---|---|
| `MultiContentsView::MultiContentsView(this, BrowserView* bv, void* arg3)` | `0x2cdf896` | `rcx=this`, `rdx=bv` → stored at `this+0x350`; `r8=arg3` (a moved-from owned ptr → `this+0x358`) |
| `EdgeContentsContainerView::EdgeContentsContainerView(this, BrowserView* bv)` | `0x2ce12b0` | the per-pane container ctor; `rdx=bv` |
| `ContentsContainerView::ContentsContainerView(this, BrowserView* bv)` | `0x2ce1b32` | base subobject ctor, called by the Edge ctor |
| `ContentsWebView::ContentsWebView(this, Profile*)` | `0x2ce2c46` | the pane's web view, lands at container `+0x3a8` |
| `edge::split_screen::EdgeMultiContentsResizeArea::EdgeMultiContentsResizeArea(this, MultiContentsView* mcv)` | `0x2ce11b0` | the one resize handle |
| `vector<raw_ptr<ContentsContainerView>>::emplace_back(this, raw_ptr*)` | `0x2ce0508` | appends to the `+0x368` vector; grows via slow path `0x2ce096c` |
| `views::View::AddChildViewAtImpl(this, View* child, size_t index)` | `0x1017902` | `r8=index` is read fresh from `this+0x1a8` each call |
| `base::internal::HandleMemorySafetyCheckedOperatorNew<3>(size)` | `0x101bd60` | allocator for **every `views::View`-derived** object; returns `rax` |
| `operator new(size)` | `0x36b3c20` | plain `new` for non-View objects (layout managers, callbacks) |
| `base::internal::RawPtrBackupRefImpl<0>::AcquireInternal(p)` | `0xf946b0` | BackupRef acquire for a raw_ptr (only if `p & 0xfffffffc00000000 == PA space`) |
| `edge::split_screen::EdgeMultiContentsResizeArea::SetVisible` | `0x2cd0f00` | |
| `views::View::SetVisible` | `0x100b6b0` | = virtual slot **`vtable+0x98`** (confirmed below) |
| `views::View::SetID` | `0x2ce22ba` | |

**Allocate-then-poison-check pattern.** Every View allocation is immediately
followed, after its ctor returns, by `cmp byte ptr [obj+0x179], 1 / je <abort>`.
`+0x179` is the allocator's memory-safety poison flag; `1` means the allocation
was rejected and the code jumps to a `ud2`/abort thunk. Injected code should use
the same `HandleMemorySafetyCheckedOperatorNew<3>` and can keep or skip the
check.

---

## 2. Object sizes (the `mov ecx, <size>` right before each `new`)

| object | size | ctor RVA | evidence (MCV/CCV ctor) |
|---|---|---|---|
| **`EdgeContentsContainerView`** (one per pane) | **`0x558`** | `0x2ce12b0` | `0x2cdfae3: mov ecx, 0x558` then `call 0x18101bd60`; again `0x2cdfc04: mov ecx, 0x558` |
| **`EdgeMultiContentsResizeArea`** | **`0x358`** | `0x2ce11b0` | `0x2cdfb8b: mov ecx, 0x358` |
| `ContentsWebView` (pane web view, → container `+0x3a8`) | `0x5b0` | `0x2ce2c46` | `0x2ce1f59: mov ecx, 0x5b0` |
| `views::WebView` (devtools web view, → container `+0x440`) | `0x540` | `0x2ce306e` | `0x2ce1d7f: mov ecx, 0x540` |
| `ScrimView` ×2 (→ `+0x448`, `+0x480`) | `0x338` | `0x2ce2366` | `0x2ce1e2f`, `0x2ce211b: mov ecx, 0x338` |
| plain `views::View` (→ `+0x3a0`) | `0x338` | `0x101de7a` | `0x2ce1ee3: mov ecx, 0x338` |
| `DataProtectionOverlayView` (→ `+0x470`) | `0x378` | `0x2ce291e` | `0x2ce2008: mov ecx, 0x378` |
| `ActorOverlayWebView` (opt, → `+0x488`) | `0x580` | `0x2ce27c4` | `0x2ce2200: mov ecx, 0x580` |
| `DelegatingLayoutManager` / `FillLayout` | `0x108` | — | `operator new`, not the checked one |

`MultiContentsView` itself is allocated by its caller (the controller), not here,
so its size is not pinned by this ctor.

---

## 3. Vtable RVAs (deliverable #3 — for reference only; the recipe never installs these)

MSVC emits one vptr per base-with-virtuals subobject, so each class stores a
*set*. The **primary** (offset-0) vtable is the one a `mov rax,[obj]; call
[rax+slot]` dispatches through; its `+0x98` slot is `views::View::SetVisible`
(verified: reading `[0x1178a5d8+0x98]` and `[0x11763498+0x98]` both yield
`views::View::SetVisible` `0x100b6b0`).

**`EdgeContentsContainerView`** (ctor `0x2ce12b0`, stores at `0x2ce12dd`…`0x2ce1360`):
- primary `[this+0]` = `0x1178a5d8` *(the main views::View table: slot0 OnPaintLayer, +0x48 ECCV dtor, +0x98 SetVisible, +0xc0 Layout)*
- `[+0x8]=0x113ef230`, `[+0x10]=0x113ef250`, `[+0x68]=0x113ef270`, `[+0x70]=0x113ef290`,
  `[+0xb0]=0x113ef2d0`, `[+0xd8]=0x113ef320`, `[+0x100]=0x113ef338`, `[+0x338]=0x113ef350`,
  `[+0x340]=0x113ef370`, `[+0x348]=0x115282d0`

**`ContentsContainerView`** (base ctor `0x2ce1b32`, before Edge overwrites):
- primary `[this+0]` = `0x11763498`
- `[+0x100]=0x113896e8`, `[+0x338]=0x11389700`, `[+0x340]=0x11389720`, `[+0x348]=0x11528190`
- also `0x11528230` (`EdgeContentsContainerViewBase` weak) transiently at `+0x348`;
  the `+8…+0xd8` slots are COMDAT-folded shared tables (named `FadeWrapper<…>`,
  `AutofillEdgePopupRowView` in the PDB — a folding artifact, same bytes)

**`EdgeMultiContentsResizeArea`** (ctor `0x2ce11b0`):
- primary `[this+0]` = `0x116a6098`
- `[+0x8]=0x1121f770`, `[+0x10]=0x1121f790`, `[+0x68]=0x1121f7b0`, `[+0x70]=0x1121f7d0`,
  `[+0xb0]=0x1121f810`, `[+0xd8]=0x1121f860`, `[+0x100]=0x1121f878`

**`MultiContentsView`** (ctor `0x2cdf896`):
- primary `[this+0]` = `0x117630f8`
- `[+0x8]=0x113895a0`, `[+0x10]=0x113895c0`, `[+0x68]=0x113895e0`, `[+0x70]=0x11389600`,
  `[+0xb0]=0x11389640`, `[+0xd8]=0x11389690`, `[+0x100]=0x113896a8`, `[+0x338]=0x115d15b8`,
  `[+0x340]=0x113896c0`, `[+0x348]=0x113896d8`

Shared metadata vtable (all three `_MetaData` classes): `0x11152b58`.

---

## 4. The container-creation block in `MultiContentsView::MultiContentsView`

The vector base pointer is cached once in `rbx`:

```
0x2cdfb0c: lea    rbx, [rsi + 0x368]        ; rbx = &this->containers_  (the +0x368 vector)
```

### Pane 0 — `this->containers_[0]`
```
0x2cdfae3: mov    ecx, 0x558                 ; sizeof(EdgeContentsContainerView)
0x2cdfae8: call   HandleMemorySafetyCheckedOperatorNew<3>
0x2cdfaed: mov    rdi, rax                   ; rdi = raw container
0x2cdfaf0: mov    rdx, [rsi + 0x350]         ; arg2 = BrowserView*
0x2cdfaf7: mov    rcx, rax
0x2cdfafa: call   EdgeContentsContainerView::EdgeContentsContainerView   ; 0x2ce12b0
0x2cdfaff: cmp    byte ptr [rdi + 0x179], 1  ; poison check
0x2cdfb06: je     <abort>
; --- parent it under the MultiContentsView ---
0x2cdfb13: mov    r8, [rsi + 0x1a8]          ; index = this View's current child count
0x2cdfb1a: mov    rcx, rsi                   ; parent = this (MultiContentsView)
0x2cdfb1d: mov    rdx, rdi                   ; child  = container
0x2cdfb20: call   views::View::AddChildViewAtImpl        ; 0x1017902
0x2cdfb34: call   RawPtrBackupRefImpl<0>::AcquireInternal (rdi)   ; conditional
; --- push into the +0x368/+0x370 vector ---
0x2cdfb3c: lea    r15, [rsp + 0x58]
0x2cdfb41: mov    [r15], rdi                 ; raw_ptr temp = container
0x2cdfb44: mov    rcx, rbx                   ; &containers_
0x2cdfb47: mov    rdx, r15
0x2cdfb4a: call   vector<raw_ptr<ContentsContainerView>>::emplace_back  ; 0x2ce0508
; --- pane-0-only fixup: mark its ContentsWebView active ---
0x2cdfb65: cmp    qword ptr [rsi + 0x370], 0 ; count>0 ?
0x2cdfb73: mov    rax, [rsi + 0x368]         ; data
0x2cdfb7a: mov    rax, [rax]                 ; containers_[0]
0x2cdfb7d: mov    rax, [rax + 0x3a8]         ; its ContentsWebView
0x2cdfb84: mov    byte ptr [rax + 0x450], 1  ; <- set only for index 0
```

### The resize area (one, between the two panes)
```
0x2cdfb8b: mov    ecx, 0x358
0x2cdfb90: call   HandleMemorySafetyCheckedOperatorNew<3>
0x2cdfb98: mov    rcx, rax
0x2cdfb9b: mov    rdx, rsi                   ; arg2 = MultiContentsView* (the delegate)
0x2cdfb9e: call   EdgeMultiContentsResizeArea::EdgeMultiContentsResizeArea  ; 0x2ce11b0
0x2cdfbb0: mov    r8, [rsi + 0x1a8]          ; AddChildViewAt(end)
0x2cdfbba: ... call views::View::AddChildViewAtImpl
0x2cdfbf3: mov    [rsi + 0x3f0], rdi         ; store the single resize handle
0x2cdfbfa: mov    rcx, rdi
0x2cdfbfd: xor    edx, edx
0x2cdfbff: call   EdgeMultiContentsResizeArea::SetVisible(this, false)    ; 0x2cd0f00
```

### Pane 1 — `this->containers_[1]`  (identical block, different fixup)
```
0x2cdfc04: mov    ecx, 0x558
0x2cdfc09: call   HandleMemorySafetyCheckedOperatorNew<3>
0x2cdfc11: mov    rdx, [rsi + 0x350]         ; BrowserView*
0x2cdfc1b: call   EdgeContentsContainerView::EdgeContentsContainerView
0x2cdfc2d: mov    r8, [rsi + 0x1a8]
0x2cdfc3a: call   views::View::AddChildViewAtImpl
0x2cdfc56: lea    r15, [rsp + 0x58]; mov [r15], rdi
0x2cdfc5e: mov    rcx, rbx; mov rdx, r15
0x2cdfc64: call   vector<...>::emplace_back                ; same 0x2ce0508, same rbx
; --- pane-1-and-beyond fixup: hide it ---
0x2cdfc7f: cmp    qword ptr [rsi + 0x370], 1 ; count>1 ?
0x2cdfc8d: mov    rax, [rsi + 0x368]
0x2cdfc94: mov    rcx, [rax + 8]             ; containers_[1]
0x2cdfc98: mov    rax, [rcx]                 ; its primary vtable
0x2cdfc9b: mov    rax, [rax + 0x98]          ; slot 0x98 = views::View::SetVisible
0x2cdfca2: xor    edx, edx                   ; false
0x2cdfca4: call   [guard_dispatch]           ; containers_[1]->SetVisible(false)
```

After the two panes, the ctor runs two **already-N-generic** loops over the whole
vector (bound = `[rsi+0x370]`, stride 8): `0x2cdfcf7` attaches an
`EdgeContentsContainerBorder` observer to each `container+0x348`, and
`0x2ce0000..0x2ce017f` registers focus callbacks + `flat_map` entries for every
container's web views. These need no change for N.

---

## 5. What each container ctor actually builds (so a clone is complete)

### `EdgeContentsContainerView::EdgeContentsContainerView` (`0x2ce12b0`, size `0x558`)
1. `0x2ce12d8` — `call ContentsContainerView::ContentsContainerView(this, bv)` (base, §5.1).
2. Overwrites all vtable slots with the `EdgeContentsContainerView` tables (§3).
3. `0x2ce137e` — `mov [this+0x510], bv` (its own BrowserView* copy, BackupRef-acquired).
4. Fills `+0x518…+0x550`; `+0x538 = 2 - BrowserView::EdgeIsAppLayerEnabled`.
5. **Conditional** (AppLayer enabled ∧ migration-to-upstream ∧ feature
   `kWebUIAgentWebcontentsOverlay`): allocates a `0x338` `views::View`
   (`SetID 0x425`, `FillLayout`), parents it, stores at **`this+0x368`** — note
   this is the *container's own* `+0x368`, unrelated to `MultiContentsView+0x368`.

### 5.1 `ContentsContainerView::ContentsContainerView` (`0x2ce1b32`, base subobject)
Runs `views::View::View`, installs base vtables, stores `bv` at `+0x398`,
embeds a `TabModalDialogHost` at `+0x3b0`, then creates and `AddChildViewAt`s a
chain of child views (each with the same alloc → ctor → poison-check →
AddChildViewAtImpl → store pattern):

| child | size | stored at | RVA of store |
|---|---|---|---|
| `views::WebView` (devtools) | `0x540` | `+0x440` | `0x2ce1e2c` |
| `ScrimView` | `0x338` | `+0x448` | `0x2ce1ea9` |
| plain `views::View` | `0x338` | `+0x3a0` | `0x2ce1f4b` |
| **`ContentsWebView`** (the pane's web view) | `0x5b0` | **`+0x3a8`** | `0x2ce1fc1` |
| `DataProtectionOverlayView` | `0x378` | `+0x470` | `0x2ce206d` |
| Indigo overlay (feat `kIndigo`) | — | `+0x4a8` | `0x2ce2105` |
| `ScrimView` | `0x338` | `+0x480` | `0x2ce2195` |
| `ActorOverlayWebView` (feat `kGlicActorUi`) | `0x580` | `+0x488` | `0x2ce2275` |

The `+0x3a8` `ContentsWebView` is the field `split-view-abi.md` pins; its `SetID`
is `0x41b` (`0x2ce1fe3`). This is the object `SetWebContentsAtIndex` later drives
via `ContentsWebView::SetWebContents`, and the one whose `+0x450` byte pane 0's
fixup sets to `1`.

### `EdgeMultiContentsResizeArea::EdgeMultiContentsResizeArea` (`0x2ce11b0`, size `0x358`)
```
0x2ce11c8: lea    rax, [rdx + 0x338]         ; delegate = mcv + 0x338 (its ResizeArea-delegate subobject)
0x2ce11d2: cmovne rdx, rax                    ;   (if mcv != null)
0x2ce11d6: call   views::EdgeTwoDimensionalResizeArea::EdgeTwoDimensionalResizeArea   ; 0x2cd1366
...install vtables (§3)...
0x2ce1242: mov    word ptr [rsi + 0x350], 0
0x2ce1250: call   EdgeMultiContentsResizeArea::SetVisible(this, false)
0x2ce126b: call   ui::PropertyHandler::SetProperty<ElementIdentifier>
0x2ce1270: movabs rax, 0x800000008            ; preferred size {8,8}
0x2ce1289: call   views::View::SetPreferredSize
```
The delegate it receives (`mcv+0x338`) is the subobject whose vtable
`MultiContentsView` installs at `+0x338`/`+0x340` — i.e. the resize area calls
back into `MultiContentsView` for drag geometry. An N-pane build needs **N−1**
of these, each still constructed with the same `mcv` pointer.

### The `+0x368` vector (`emplace_back` `0x2ce0508`)
Layout is the raw_ptr variant `{ begin_ptr @ +0x368, size_t count @ +0x370,
size_t capacity @ +0x378 }`:
```
0x2ce0520: mov    rcx, [rcx + 8]             ; count
0x2ce0524: lea    rax, [rcx*8]; add rax,[rsi]; ... end = begin + count*8
0x2ce0557: cmp    rcx, [rsi + 0x10]          ; count == capacity ?  -> grow (slow path 0x2ce096c)
0x2ce0576: mov    [rsi + 8], rdx             ; count = (end-begin)/8  after insert
```
So appending is just: call `emplace_back(&vector, &raw_ptr)`; it handles growth.
This confirms the `+0x370 = count` fact and that nothing caps the count at 2 —
the 2 is purely how many times the ctor calls the block.

---

## 6. The recipe: create `container[i]` for arbitrary i

For each new pane index `i` (beyond the two the ctor makes), an injected routine
run against a live `MultiContentsView* mcv` does exactly what the block above does:

1. **Allocate** `p = HandleMemorySafetyCheckedOperatorNew<3>(0x558)`  *(RVA `0x101bd60`)*.
2. **Construct** `EdgeContentsContainerView::EdgeContentsContainerView(p, *(BrowserView**)(mcv+0x350))`  *(RVA `0x2ce12b0`)*.
   This builds the whole child chain incl. the `ContentsWebView` at `p+0x3a8`
   and installs every vtable. Optionally `cmp byte[p+0x179],1` and bail on poison.
3. **Parent** it: `AddChildViewAtImpl(mcv, p, *(size_t*)(mcv+0x1a8))`  *(RVA `0x1017902`)* — index = the View's current child count, i.e. append.
4. **BackupRef-acquire** (optional, matches ctor): if `p & 0xfffffffc00000000`
   equals the PA-space constant at `[0x140f5b48]`, call `AcquireInternal(p)` *(`0xf946b0`)*.
5. **Append to the vector**: store `p` into an 8-byte stack temp and call
   `emplace_back(mcv+0x368, &temp)`  *(RVA `0x2ce0508`)*. `count` at `mcv+0x370`
   becomes `i+1` automatically.
6. **Per-index fixup**: for the pane that should be hidden initially (every pane
   but the visible one), `container[i]->SetVisible(false)` via vtable `+0x98`
   (`views::View::SetVisible`, `0x100b6b0`) — the exact thing pane 1 does. The
   pane that is "primary" gets `*(ContentsWebView**)(container+0x3a8) + 0x450 = 1`
   instead (pane 0's fixup).
7. **Resize areas**: to keep neighbours draggable, allocate a `0x358`
   `EdgeMultiContentsResizeArea` per *gap* (`new` → ctor `0x2ce11b0` with
   `rdx=mcv` → `AddChildViewAtImpl(mcv, area, end)`), as the ctor does once for
   `mcv+0x3f0`. **This is the one storage slot that is not yet a vector** — the
   ctor keeps a single pointer at `+0x3f0`, so an N-pane build needs its own
   `area[]` store and the geometry code (`CalculateSeparatorLayouts`, see
   `split-view-abi.md`) taught to place N−1 of them. For a first "just make a
   third pane render" cut you can skip extra resize areas; the third pane will
   appear but sit without a draggable divider.

Nothing in steps 1–5 is index-specific, which is why "one more pane" is
mechanically a third invocation of the same block.

---

## 7. Uncertainties / things to re-check before shipping the injector

- **`+0x428` meaning.** The ctor constructs a `split_tabs::SplitTabVisualData`
  in place at `mcv+0x428` (`0x2cdf9a0: lea rcx,[rsi+0x428]` … `0x2cdfa0f: call
  SplitTabVisualData::SplitTabVisualData`). `split-view-abi.md` calls `+0x428`
  the "active pane index" (read by `ShowSplitView`/`GetViewSizes`). Both can be
  true if the active index is the first field of `SplitTabVisualData`, but I did
  **not** disassemble that struct's ctor to confirm the exact sub-offset. Flagging
  it because an N-pane active-index writer must target the right field.
- **`ContentsWebView+0x450` byte.** Pane 0 sets it to `1`; its precise semantics
  (active/primary/visible) weren't traced into `ContentsWebView`. Treat as
  "primary-pane flag" and set it on whichever pane is active.
- **`View+0x1a8` as the child-insertion index.** Confirmed only behaviourally:
  it is re-read before every `AddChildViewAtImpl` and the children clearly append
  in order. It is the `views::View` children-count/size field; I did not dump the
  `views::View` layout to prove the field name. Either way "append at end" holds.
- **BackupRef gate.** The `and reg, 0xfffffffc00000000 / cmp [0x140f5b48]` guard
  decides whether `AcquireInternal` runs; `[0x140f5b48]` is
  `PartitionAddressSpace::setup_+0x8`. If the injected allocation isn't in the
  PA BRP pool the acquire is skipped — matching the ctor's own conditional, so
  copying the guard verbatim is safe.
- **Allocator choice.** Using plain `operator new(0x558)` instead of the checked
  allocator would skip the `+0x179` poison initialization; prefer
  `HandleMemorySafetyCheckedOperatorNew<3>` to stay bit-identical to the ctor.
