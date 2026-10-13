# Edge split-view — wiring real web content into a GROWN pane

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`), via `tools/re/{symbols,disasm}.py` + the scratchpad
`drange.py`/`xref.py`. Every RVA/offset is pinned to this build; re-verify after
an Edge update. Builds on `docs/re/split-view-abi.md` and `docs/re/controller.md`.

> **The question.** Our injector already grows the `MultiContentsView` container
> vector past 2 and tiles the geometry, but a grown pane (container index ≥ 2) is
> blank. What is the exact minimal call sequence that puts a real page into it?

---

## 0. Objects we already hold

| What | How to reach it | Evidence |
|---|---|---|
| `MultiContentsView*` (MCV) | `*(void**)(BrowserView + 0x6e0)` | `ShowSplitView`: `mov r12,[rsi+0x6e0]` then used as MCV in the feed call |
| `BrowserView*` | MCV `+0x350` (back-ptr, prior RE) / or the `this` of our `ShowSplitView` hook | split-view-abi.md |
| container vector begin | `*(void***)(MCV + 0x368)` | `SetWebContentsAtIndex`: `mov rcx,[rdi+0x368]` |
| container vector count | `*(size_t*)(MCV + 0x370)` | `SetWebContentsAtIndex`: `cmp [rcx+0x370],rax` bounds check |
| container[i]'s `ContentsWebView*` | `*(void**)(container[i] + 0x3a8)` | `SetWebContentsAtIndex`: `mov rcx,[rax+0x3a8]` |
| `Browser*` | `*(void**)(BrowserView + 0x600)` | `ShowSplitView`/`GetActiveWebContents`: `[bv+0x600]`→`[+0x30]` is the TabStripModel |
| `TabStripModel*` | `*(void**)(Browser + 0x30)` | `Browser::GetTabStripModel` `0x55ba830`: `mov rax,[rcx+0x30]; ret` |

---

## 1. `MultiContentsView::SetWebContentsAtIndex` — RVA `0xc930f18`

ABI: `__fastcall(this=rcx MCV*, rdx content::WebContents* wc, r8d int index)`.

### The hard cap (the thing that crashes a grown pane)

```
0xc930f1f: cmp    r8d, 2            ; 41 83 F8 02   (immediate 02 at 0xc930f22)
0xc930f23: jae    0x18c930ff7       ; 0F 83 CE 00 00 00  -> int3/ud2
...
0xc930ff7: int3
0xc930ff8: ud2
```

Raw bytes at `0xc930f18`: `56 57 53 48 83 EC 20 41 83 F8 02 0F 83 CE 00 00 00 ...`

**Neutralize, two ways (only needed if we call this fn at all — see §4):**
- **1-byte patch:** at RVA `0xc930f22` write `0xFF` over `0x02` → `cmp r8d, 0xFF`
  (raises the cap to 255, keeps the real out-of-range `ud2`).
- **6-byte NOP:** at `0xc930f23` overwrite `0F 83 CE 00 00 00` with six `0x90`.

### What it does for a VALID index (the wiring we want)

```
0xc930f32: cmp    qword ptr [rcx + 0x370], rax   ; bounds: index < count, else ud2
0xc930f3f: mov    rsi, rdx                        ; rsi = wc
0xc930f42: mov    rcx, qword ptr [rdi + 0x368]    ; containers.begin()
0xc930f49: mov    rax, qword ptr [rcx + rax*8]    ; containers[index]
0xc930f4d: mov    rcx, qword ptr [rax + 0x3a8]    ; containers[index]->ContentsWebView*
0xc930f54: call   0x182b57520                     ; -> ContentsWebView::SetWebContents(cwv, wc)
```

So the entire render-wiring is one call: `ContentsWebView::SetWebContents(container[index]+0x3a8, wc)`.

### What else it touches (and why index ≥ 2 is clean)

- **`cmp ebx,1 / jne +0x68`** (`0xc930f59`) — the resize-area + container-`SetVisible`
  block at `+0x9e` runs **only for index == 1**. For index ≥ 2 it is skipped
  entirely, so the resize area at MCV `+0x3f0` is **not** touched.
- Tail at `+0x68`: if `wc != null`, `TabInterface::MaybeGetFromContents(wc)` →
  `SadTabHelper::From` → `ReinstallInWebView`. Cosmetic (sad-tab), not required to render.
- It does **NOT** write the SplitTabVisualData at MCV `+0x428`, nor the per-container
  active flag at container `+0x38d`, nor the CWV active flag. Those are set by
  `UpdateSplitVisualData`/`SetActiveIndex` from the feeder (§3), and only drive the
  active-pane border/highlight — not whether a page shows.

### `ContentsWebView::SetWebContents` — RVA `0x2b57520`

ABI: `__fastcall(this=rcx ContentsWebView*, rdx content::WebContents* wc)`.

```
0x2b5752d: mov    rcx, rdx
0x2b57530: call   ...OverlayViewTabHelper::HiddenByOverlayView
0x2b57540: mov    rdx, rdi                 ; rdx = wc
0x2b57543: call   0x182b57680              ; -> views::WebView::SetWebContents(this, wc)   <-- real attach
```

`views::WebView::SetWebContents` (`0x2b57680`) is what attaches the `WebContents`
to the view's native host and makes it render. Everything after it in
`ContentsWebView::SetWebContents` is status-bubble / modal-dialog bookkeeping.

---

## 2. Getting a `content::WebContents*` to feed

Two resolved getters, both reachable from objects we hold:

| Fn | RVA | ABI | Returns |
|---|---|---|---|
| `TabStripModel::GetWebContentsAt` | `0x101530e` | `(rcx tsm, edx int index)` | `WebContents*` at strip index |
| `TabStripModel::GetActiveWebContents` | `0xf97532` | `(rcx tsm)` | active tab's `WebContents*` |
| `BrowserView::GetActiveWebContents` | `0xc934c14` | `(rcx BrowserView)` | active tab's `WebContents*` (one call) |

`GetWebContentsAt` (`0x101530e`): `test edx,edx/js ud2`, `cmp edx,[[rcx+0x10]+0x18]/jge ud2`
(bounds 0 ≤ index < count), then `TabCollection::GetTabAtIndexRecursive` → returns the
tab's `WebContents*` via an icall. Clean, bounds-checked.

`BrowserView::GetActiveWebContents` is literally `browser()->tab_strip_model()->GetActiveWebContents()`:
`[bv+0x600]`→`[+0x30]` (TabStripModel) →`[+0x98]`→ icall `[+0x18]` — identical body to
`TabStripModel::GetActiveWebContents`.

> **Correctness trap.** A `WebContents` lives in exactly ONE `WebView` at a time;
> attaching it to a second detaches it from the first. The **active** tab's
> `WebContents` is already shown in pane 0 or 1, so feeding *that* to pane 2 would
> steal it. For a grown pane feed a **different, currently-not-displayed tab**:
> `GetWebContentsAt(tsm, idx)` for a background tab (not the active/split tabs).
> Background tabs are not attached to any `ContentsWebView`, so this is safe and
> renders that page in the new pane.

---

## 3. Does the grown pane need N tabs in the SplitTabData? — the feeder

`BrowserView::ShowSplitView` (`0xc93594a`) is the **only** caller of
`SetWebContentsAtIndex` (`xref.py` over `0xc930f18` → single hit at
`ShowSplitView+0x222`). Its feed loop:

```
0xc935a1a: call   0x188184fe0                  ; split_tabs::SplitTabData::ListTabs(splitData, &vec)
0xc935a1f: mov    rax, qword ptr [rdi + 8]      ; rax = tab COUNT from ListTabs
0xc935a23: lea    r13, [rax*8]                  ; loop bound = count*8
...
0xc935b43: mov    rbx, qword ptr [rsp + 0x30]   ; tabs.begin()
0xc935b48: xor    r15d, r15d                    ; i = 0
0xc935b4b: mov    rcx, qword ptr [rbx + r15*8]  ; tabs[i]  (Tab*)
0xc935b4f: mov    r12, qword ptr [rsi + 0x6e0]  ; MCV*
0xc935b56: mov    rax, qword ptr [rcx]          ; vtable
0xc935b59: mov    rax, qword ptr [rax + 0x18]   ; vtable+0x18 = TabInterface::GetContents
0xc935b5d: call   _guard_dispatch_icall_fptr    ; rax = tabs[i]->GetContents()  (WebContents*)
0xc935b66: mov    rdx, rax                      ; wc
0xc935b69: mov    r8d, r15d                     ; index = i
0xc935b6c: call   0x18c930f18                   ; SetWebContentsAtIndex(MCV, wc, i)
0xc935b71: inc    r15 ; add r13,-8 ; jne loop
```

**Decisive: the feeder loops exactly `ListTabs()`-count times.** `ListTabs`
(`0x8184fe0`) is `mov rcx,[rcx]; jmp TabCollection::GetTabsRecursive` — it returns
the tabs of the split's underlying `TabCollection`. A real split has 2 tabs, so the
loop runs for i = 0,1 only. **It will never feed index ≥ 2**, regardless of how many
containers exist. It also does not clear/overwrite index ≥ 2 (loop stops at count).

Consequences:
- Growing only the container vector + neutralizing the guard still leaves pane 2
  blank — nothing feeds it.
- A grown pane fed directly by us (§4a) is **not** overwritten by later
  `ShowSplitView`/relayout, because the feeder's bound stays at 2. Relayout
  (`CalculateProposedLayout`/`GetViewSizes`) only moves geometry; it never nulls a
  CWV's `WebContents`. (The only code that nulls a CWV is `CloseSplitView`, and only
  for container[1]: `mov rax,[rsi+0x368]; mov rcx,[rax+8]; ... xor edx,edx; call ContentsWebView::SetWebContents`.)

### If you insist on the native path (approach b): `TabStripCollection::CreateSplit` — `0x818834e`

ABI: `__fastcall(rcx this, rdx, r8 std::vector<...>* tabs, r9)`. Guard:
`cmp qword ptr [r8+8], 1 / jbe bail` — requires **>1** tab but does **not** cap at 2;
it loops over all entries of the `tabs` vector (`r12 = [r8+8]*8`) and `operator new(0x98)`
allocates the `SplitTabData`. So it *can* build a 3-tab split, but it re-parents real
tabs into a new split collection and fires the full TabStripModel observer cascade.
There is no cheap "add one tab to an existing `SplitTabData`" entry point — membership
lives in the collection at `SplitTabData+0` (`ListTabs` reads `[this]`), so growing an
existing split means `TabCollection` surgery (add/move) with the same observer blast
radius. High risk.

---

## 4. Recommendation

**Use approach (a): feed the grown container's `ContentsWebView` directly. Do NOT
touch the SplitTabData.** It is self-contained on the UI thread, needs no tab-model
mutation, and is not undone by relayout. The cleanest variant needs **no binary
patch at all** because it bypasses `SetWebContentsAtIndex` (and its `index<2` guard)
entirely:

### (a) Direct CWV feed — recommended, no patch

Per grown container index `i` (≥ 2), on the UI thread, after our tiling code has
added container[i] to the vector and made it visible/laid-out:

1. `Browser* br   = *(void**)(browserView + 0x600);`
2. `void*  tsm    = *(void**)(br + 0x30);`                      // TabStripModel
3. `WebContents* wc = GetWebContentsAt(tsm, bgIndex);`          // RVA `0x101530e`, rcx=tsm, edx=index — a background tab, NOT active/split
4. `void* cwv    = *(void**)(container[i] + 0x3a8);`            // ContentsWebView*
5. `ContentsWebView::SetWebContents(cwv, wc);`                  // **RVA `0x2b57520`**, rcx=cwv, rdx=wc

Notes:
- `SetWebContentsAtIndex` does `container[1]->SetVisible(true)` only for index 1, so
  for index ≥ 2 **we** must ensure `container[i]` is visible (our tiling layout
  already governs this). The CWV feed itself does not set visibility.
- Optional cosmetic parity (sad-tab reinstall + active border) is what the tail of
  `SetWebContentsAtIndex` and `UpdateSplitVisualData`/`SetActiveIndex` add; skip for a
  first cut.

### (a′) Via `SetWebContentsAtIndex` — if you want its sad-tab reinstall / symmetry

One-time: patch RVA `0xc930f22` `0x02`→`0xFF` (or NOP the `jae` at `0xc930f23`).
Then per grown index `i`:

1–3. same as above to get `wc` (background tab).
4. `MultiContentsView::SetWebContentsAtIndex(MCV, wc, i);` — **RVA `0xc930f18`**,
   rcx=MCV, rdx=wc, r8d=i. (For i ≥ 2 the index==1 resize-area branch is skipped, so
   nothing else in MCV state is disturbed.)

### (b) Grow the SplitTabData — NOT recommended

`CreateSplit` (`0x818834e`) / `TabCollection` surgery to make `ListTabs` return 3 so
the native feeder populates index 2. Self-maintaining but touches real tab-strip
state, fires the observer cascade (toolbar, omnibox, session, tab chrome that assumes
2), and `CreateSplit` re-parents tabs + allocates a fresh `SplitTabData`. Save for a
later, fully-native implementation.

---

## Quick RVA reference (this build)

| Symbol | RVA | ABI (args) |
|---|---|---|
| `MultiContentsView::SetWebContentsAtIndex` | `0xc930f18` | rcx=MCV, rdx=WebContents*, r8d=index |
| &nbsp;&nbsp;↳ `index<2` guard immediate byte | `0xc930f22` | patch `02`→`FF` |
| `ContentsWebView::SetWebContents` | `0x2b57520` | rcx=ContentsWebView*, rdx=WebContents* |
| `views::WebView::SetWebContents` | `0x2b57680` | rcx=WebView*, rdx=WebContents* |
| `BrowserView::ShowSplitView` (only feeder caller) | `0xc93594a` | rcx=BrowserView*, dl=flag |
| `BrowserView::GetActiveWebContents` | `0xc934c14` | rcx=BrowserView* |
| `TabStripModel::GetWebContentsAt` | `0x101530e` | rcx=TabStripModel*, edx=index |
| `TabStripModel::GetActiveWebContents` | `0xf97532` | rcx=TabStripModel* |
| `Browser::GetTabStripModel` | `0x55ba830` | rcx=Browser* → `[+0x30]` |
| `split_tabs::SplitTabData::ListTabs` | `0x8184fe0` | rcx=SplitTabData*, rdx=out vec |
| `tabs::TabStripCollection::CreateSplit` | `0x818834e` | rcx=this, rdx, r8=vec(tabs), r9 |

Key offsets: BrowserView `+0x600`=Browser*, `+0x6e0`=MCV*; Browser `+0x30`=TabStripModel;
MCV `+0x368`=containers.begin, `+0x370`=count, `+0x3f0`=resize area, `+0x428`=SplitTabVisualData;
container `+0x3a8`=ContentsWebView*, `+0x38d`=active flag.
