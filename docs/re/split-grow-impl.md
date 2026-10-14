# Grow a live split 2→3 — the exact native call sequence

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`). Every `0x…` is an **RVA** unless written `0x18…`
(linear, as capstone prints call targets); `linear = 0x180000000 + rva`.
Produced with `tools/re/{disasm,symbols}.py` + scratch `drange.py`. Re-verify
after an Edge update. Builds on `split-insert.md`, `controller.md`,
`tab-wiring.md`.

> **Goal.** An injected DLL on the UI thread, holding `TabStripModel*`, wants to
> add **one existing background tab** (given its strip index) to the **active
> split**, turning a 2-member split into a 3-member split, so Edge's own
> lifecycle feeds and controls all three panes.

> **Bottom line (decided by disasm, not guessed).** You **cannot** append to a
> live split. `TabStripModel::AddToSplitImpl` (`0xa273e30`) is *re-create from a
> full member list*, and it executes `int3/ud2` on **any** member tab that is
> **already in a split** (per-index `TabModel::IsSplit` check). The existing two
> members are in the split, so feeding `AddToSplitImpl` the full list while the
> split is live crashes; feeding it only the new index fails `CreateSplit`'s
> `size>1` guard and creates nothing. The one correct path is the transaction
> Edge itself runs in `UpdateTabInSplitImpl`: **capture id + visual →
> `RemoveSplitImpl(id, flag=1)` (ejects all members back to standalone tabs) →
> rebuild a full `vector<int>` of all three members' *post-remove* indices →
> `AddToSplitImpl(sameId, vec3)`.** Its tail fires `NotifySplitTabCreated`, which
> re-runs `BrowserView::ShowSplitView`, whose `ListTabs()`-bounded loop now feeds
> pane index 2.

---

## 1. Get the active split's id (`base::Token`)

### Token size / layout — **16 bytes** = `base::Token` (2× u64); `SplitTabId` *is* a `base::Token`

Proven by `tabs::TabModel::GetSplit` (`0x34260d0`), the whole function:

```
0x34260d0: mov    rax, rdx                       ; rax = out ptr (optional<SplitTabId>*)
0x34260d3: mov    rdx, [rcx + 0x78]              ; has_value byte  (TabModel+0x78)
0x34260d7: mov    [rax + 0x10], rdx              ; -> out+0x10
0x34260db: movups xmm0, [rcx + 0x68]             ; the Token, 16 bytes (TabModel+0x68)
0x34260df: movups [rax], xmm0                    ; -> out+0
0x34260e2: ret
```

So `std::optional<split_tabs::SplitTabId>` as returned here is a **17+-byte**
out-struct: **`base::Token` (16 bytes) at `+0`, `bool has_value` at `+0x10`**.
Every id copy in the split code is a 16-byte `movups xmm` (also
`base::Token::CreateRandom` fills 16 bytes in `AddToNewSplit` at `0xa273a72`).
The split id lives **on the `TabModel` at `+0x68`** (Token) / `+0x78`
(presence), *not* on `SplitTabData`.

### The reads/calls

`active tab = *(TabStripModel + 0x98)` (a `tabs::TabModel*` /`TabInterface*`;
confirmed `mov r8,[r15+0x98]` in `AddToNewSplit` `0xa27399d` and
`mov r8,[rsi+0x98]` in `UpdateTabInSplitImpl` `0xa273576`).

Two equivalent ways to read the id, both proven:

1. **Virtual call on the active tab** (what `UpdateTabInSplitImpl` does,
   `0xa273054`):
   ```
   OptSplit opt;                         // 0x18 bytes: Token@0, bool@0x10
   (*(fn**)(*(void**)activeTab)[0x118/8])(activeTab, &opt);   // tab->GetSplit(&opt)
   if (!opt.present /* opt bytes[0x10] */) return;            // active tab not split
   base::Token id = *(base::Token*)&opt;                      // 16 bytes
   ```
   `vtable+0x118 = tabs::TabModel::GetSplit`. (Also usable:
   `vtable+0x108 = tabs::TabModel::IsSplit` → `bool`, see §2.)

2. **`TabStripModel::GetSplitForTab` (`0x2b2a682`)** — a thin wrapper, ABI
   `(rcx=tsm, rdx=out OptSplit*, r8d=int tabIndex)`:
   ```
   0x2b2a690: mov  rcx,[rcx+0x10]              ; root collection
   0x2b2a694: cmp  r8d,[rcx+0x18] ; jge <oob>  ; bounds-check the index
   0x2b2a6a4: call GetTabAtIndexRecursive      ; index -> TabModel*
   0x2b2a6ac: mov  r8,[rcx+0x118]              ; tab->GetSplit
   0x2b2a6bc: call _guard_dispatch             ; writes OptSplit at rdx
   0x2b2a6c2: mov  rax,rsi ; ret               ; returns the out ptr
   ```
   Needs the active tab's **index** (from the menu context, or
   `GetIndexOfTabRecursive`/`GetActiveTab 0xfd2e60`).

### The visual data of the live split — read `SplitTabData+8`

`TabStripModel::GetSplitData` (`0xa2677a4`), ABI `(rcx=tsm, rdx=base::Token* id)`
→ returns `split_tabs::SplitTabData*`:

```
0xa2677b7: mov    rcx,[rcx+0x10]               ; root collection
0xa2677bb: movups xmm0,[rdx]                   ; load 16-byte id
0xa2677c6: call   GetSplitTabCollection        ; id -> SplitTabCollection* (or null->int3)
0xa2677d0: mov    rax,[rax+0x90]               ; SplitTabData = SplitTabCollection+0x90
0xa2677ef: ret
```

The 16-byte `{int layout @+0, double ratio @+8}` visual lives at
**`SplitTabData+8`** (proven in `UpdateTabInSplitImpl` `0xa273105`:
`mov rax,[rax+0x90]; movups xmm0,[rax+8]` → saved and reused as the
`AddToSplitImpl` visual arg).

---

## 2. `AddToSplitImpl` (`0xa273e30`) — full, register/stack-pinned signature

`__fastcall`, 4 register + 3 stack args. Prologue (`0xa273e30`):

```
push r15 r14 r13 r12 rsi rdi rbp rbx   ; 8 pushes = 0x40
sub  rsp, 0x368                        ; frame; total adjust from entry = 0x3a8
0xa273e4b: mov r15, r9                 ; r9  = vector<int>* (members)
0xa273e4e: mov rdi, r8                 ; r8  = base::Token* (split id)
0xa273e51: mov [rsp+0x38], rdx         ; rdx = out base::Token* (see tail)
0xa273e56: mov rbx, rcx                ; rcx = this (TabStripModel*)
```

| reg / stack | type | meaning — evidence |
|---|---|---|
| `rcx` | `TabStripModel*` | `this`. `[rcx+0x10]` = root `TabStripCollection` (used everywhere below). |
| `rdx` | **out** `base::Token*` (16B) | `mov [rsp+0x38],rdx`; **tail writes the resulting id here** (`0xa274365: mov rsi,[rsp+0x38]; movups [rsi],xmm0`) and returns `rax=rsi`. Not mere scratch — it receives the split's Token. Pass a valid 16-byte buffer. |
| `r8` | `base::Token*` (16B) | the **split id**. Copied into `CreateSplit`'s `rdx` (`0xa273f94: movups xmm0,[rdi]; …[rsp+0x320]=xmm0`). Caller picks: random (new) or an existing id (preserve identity). |
| `r9` | `std::vector<int>*` = `SplitIndexVec{int* @0, size @8, cap @16}` | **member strip indices, looped in FULL, no cap**: `0xa273e82: mov r13,[r9+8]; shl r13,2` (loop bound = size×4). |
| `[rsp+0x20]` (arg5) | `int` **anchor** | lands at `[rsp+0x3d0]`: `0xa273ef2: mov ebp,[rsp+0x3d0]; test ebp,ebp; js <ud2>; cmp ebp,[coll+0x18]; jge <ud2>` → must be `0 ≤ anchor < tabCount`. Fed to `MoveTabsAndSetPropertiesImpl` as the base index the members are gathered to. |
| `[rsp+0x28]` (arg6) | `SplitTabVisualData*` (16B) | lands at `[rsp+0x3d8]`: `0xa273f0e: mov rsi,[rsp+0x3d8]`; its 16 bytes are copied (`0xa273f80: movups xmm0,[rsi]`) into `CreateSplit`'s `r9`. Orientation + ratio. |
| `[rsp+0x30]` (arg7) | `int` **flag** | lands at `[rsp+0x3e0]`: `0xa274257: mov ebp,[rsp+0x3e0]` → passed as `r9d` to `NotifySplitTabCreated` (`0xa27435a`). `0` from `AddToNewSplit`, `1` from `UpdateTabInSplitImpl`. |

(Stack-arg offsets: entry→body shift is `0x3a8` = 0x40 pushes + 0x368 sub, so
caller `[rsp+0x20/0x28/0x30]` ⇒ callee `[rsp+0x3d0/0x3d8/0x3e0]`.)

### Body — it is *re-create from a full list*, and it rejects already-split tabs

```
; ---- member loop: resolve each index, REFUSE if already in a split ----
0xa273e82: r13 = [r9+8] << 2                      ; = size*4 (ALL indices)
0xa273ea3:   rcx=[rbx+0x10]; call GetTabAtIndexRecursive(idx)   ; int -> TabModel*
0xa273eb7:   mov rax,[rcx]; mov rax,[rax+0x108]   ; tab->vtable[0x108] = TabModel::IsSplit
0xa273ec1:   call _guard_dispatch_icall           ; bool IsSplit()
0xa273ec7:   test al,al
0xa273ec9:   jne  0x18a274403                     ; *** IsSplit==true -> int3/ud2 (CRASH) ***
0xa273ed5:   emplace_back<TabInterface*>          ; build vector<TabModel*>
; ---- anchor bounds, gather tabs contiguous, then create ----
0xa273f16: IndexOfFirstNonPinnedTab
0xa273f77: call MoveTabsAndSetPropertiesImpl(this, r9=idxVec, r8d=anchor, &flags)
            ; ^ MOVES the given strip-index tabs to be contiguous around anchor
0xa273fad: call tabs::TabStripCollection::CreateSplit(coll=[this+0x10], &id, &tabVec, &visual)
…selection/observer bookkeeping…
0xa27435d: call TabStripModel::NotifySplitTabCreated(this, &selState, r9d=flag)  ; VIEW feeder
0xa274365: mov rsi,[rsp+0x38]; movups [rsi],xmm0  ; *** write resulting id into rdx out ***
```

- `vtable+0x108` is **`tabs::TabModel::IsSplit` (`0x2eeb530`)**, whose entire body
  is `mov al,[rcx+0x78]; ret` — i.e. the same `has_value` byte `GetSplit` copies.
  So the per-index check is literally "is this tab already in a split?".
- The only `int3/ud2` sites are `0xa274403` (IsSplit==true) and `0xa274406`
  (anchor out of range). **There is no member-count cap** anywhere
  (no `cmp …,2`).
- `CreateSplit` (`0x818834e`) always makes a **fresh** `SplitTabCollection` and
  `MoveTabImpl`s the member tabs into it; its only guard is
  `0x818836d: cmp [r8+8],1; jbe <bail>` → **member vector size must be > 1**.
- **Yes, `AddToSplitImpl` moves the given strip-index tabs** (via
  `MoveTabsAndSetPropertiesImpl` then `CreateSplit`→`MoveTabImpl`); the caller
  does not need to pre-arrange contiguity. It takes the **full new member list**,
  not a delta.

### Caller 1 — `AddToNewSplit` (`0xa273926`) pins the stack-arg layout (flag 0)

```
0xa273a72: call base::Token::CreateRandom         ; rsi = fresh id
0xa273a8a: mov [rsp+0x20], r12d                   ; arg5 = indices[0] (anchor)
0xa273a85: mov [rsp+0x28], rax ; rax=&visualCopy  ; arg6 = SplitTabVisualData*
0xa273a8f: mov [rsp+0x30], 0                      ; arg7 = flag 0  (new split)
0xa273a97: lea rdx,[rsp+0xf0]                     ; rdx = out Token buffer
0xa273a9f: mov rcx,r15 (this) ; r8=rsi(id) ; r9=rbx(vector<int>)
0xa273aa8: call AddToSplitImpl
```
(Upstream it caps at 1–2: `0xa273988: cmp (size-1),2; jae bail` — so it is
**not** a growth path, and it mints a new id.)

### Caller 2 — `UpdateTabInSplitImpl` (`0xa273000`) is the growth template (flag 1)

```
0xa2736a6: mov [rsp+0x28], rax ; rax=&visual(reused SplitTabData+8)   ; arg6
0xa2736ab: mov [rsp+0x20], ebp ; active-tab index                      ; arg5 (anchor)
0xa2736af: mov [rsp+0x30], 1                                           ; arg7 = flag 1
0xa2736c7: mov rcx,rsi(this) ; rdx=&scratch16 ; r8=&id(preserved) ; r9=&vecN
0xa2736d3: call AddToSplitImpl
```

---

## 3. Simplest add path — **(b) is the only correct one**

`AddToSplitImpl` is reached three ways (xref): `AddToNewSplit+0x182`
(`0xa273aa8`), `UpdateTabInSplitImpl+0x6d3` (`0xa2736d3`), `RestoreSplit+0xc7`
(`0xa2744d1`). Evaluating the two candidate growth paths:

- **(a) `AddToSplitImpl(sameId, {idx0, idx1, newIdx})` with the split still live
  → CRASH.** The loop resolves `idx0`/`idx1` to their `TabModel*`, each of which
  is still in the split, so `IsSplit()` returns true and
  `0xa273ec9: jne 0x18a274403 (int3/ud2)` fires. Proven by `IsSplit` reading
  `[tab+0x78]` and `GetSplit` setting it.
- **(a′) `AddToSplitImpl(sameId, {newIdx})` (just the new tab) → nothing
  happens.** The single member passes `IsSplit`, but `CreateSplit`'s
  `cmp [r8+8],1; jbe bail` rejects a size-1 vector; no collection is created, and
  the existing split is untouched (worse: the tab was already moved by
  `MoveTabsAndSetPropertiesImpl`). `CreateSplit` **always** builds a *new*
  collection anyway, so there is no "add into the existing collection" semantics.
- **(b) `RemoveSplitImpl(id, 1)` + `AddToSplitImpl(sameId, vec3)` → works.** After
  the remove, all three tabs are standalone (`IsSplit()==false`), so the loop
  passes; `vec3.size()==3 > 1` passes `CreateSplit`; the same id rebinds the
  rebuilt collection. This is exactly `UpdateTabInSplitImpl`.

### Concrete ordered calls (add background tab at strip index `B` to the active split)

```c
// --- preconditions: UI thread; tsm = TabStripModel*; B = background tab's current index ---

// 0) active tab + its split id
TabModel* active = *(TabModel**)((char*)tsm + 0x98);
struct { base::Token tok; bool present; } opt = {};      // 0x18 bytes, zeroed
(*(void(**)(void*,void*))((*(void***)active)[0x118/8]))(active, &opt);   // active->GetSplit
if (!opt.present) return;                                 // active tab isn't in a split
base::Token id = opt.tok;                                 // 16 bytes

// 1) current visual (preserve orientation + ratio)
SplitTabData* sd = GetSplitData(tsm, &id);                // 0xa2677a4
char visual[16]; memcpy(visual, (char*)sd + 8, 16);       // {int layout, double ratio}

// 2) capture the current member TAB POINTERS (indices shift after remove).
std::vector<std::pair<TabModel*,int>> members;            // 24-byte vector, zeroed
GetTabsAndIndicesInSplit(tsm, &members, &id);             // 0xa269a50  (see shape below)
TabModel* newTab = GetTabAtIndexRecursive(*(void**)((char*)tsm+0x10), B);  // 0x180fb86b2
//   (newTab must NOT already be in a split; verify newTab->IsSplit()==false first)

// 3) dissolve the live split; flag 1 = transactional remove (members -> standalone tabs)
RemoveSplitImpl(tsm, &id, /*r8d*/1);                      // 0xa26df6a

// 4) re-resolve every member's POST-REMOVE index via GetIndexOfTabRecursive (0x180fb8408),
//    build a sorted std::vector<int> idx3 = {all 3 members}. Allocate its int[3] buffer
//    with operator new (0x36b3c20) so Edge may realloc/free it.
int anchor = idx3[0];                                     // smallest member index, in range

// 5) re-create under the SAME id with the full 3-member list
base::Token outId;                                        // 16-byte out buffer
AddToSplitImpl(tsm, /*rdx*/&outId, /*r8*/&id, /*r9*/&idx3,
               /*[rsp+0x20]*/anchor, /*[rsp+0x28]*/(SplitTabVisualData*)visual,
               /*[rsp+0x30]*/1);                           // 0xa273e30
// NotifySplitTabCreated -> BrowserView::ShowSplitView feeds panes 0,1,2.
```

Re-resolving indices *after* the remove mirrors `UpdateTabInSplitImpl`
(`0xa273504`–`0xa273576` rebuilds the `vector<int>` from the saved member
pointers via `GetIndexOfTabRecursive`). Do **not** reuse pre-remove indices — the
remove can shift them, and a stale index could resolve to the wrong (possibly
already-split) tab and trip the `ud2`.

### `GetTabsAndIndicesInSplit` (`0xa269a50`) signature + output shape

ABI `(rcx=tsm, rdx=out std::vector<std::pair<TabModel*,int>>*, r8=base::Token* id)`.

```
0xa269a7d: movups [rdx],xmm6; mov [rdx+0x10],0     ; zero-init out vector (24 bytes)
0xa269a88: movups xmm0,[r8]                         ; load 16-byte id
0xa269a98: call GetSplitTabCollection               ; null -> 0xa269b9e int3/ud2
0xa269aa6: mov rcx,[rax+0x90]; call SplitTabData::ListTabs
… per member: GetIndexOfTabRecursive(first), then emplace_back{ first=TabModel*, second=idx++ }
0xa269b78: mov rax,rsi; ret                         ; returns the out vector ptr
```

Output: `std::vector<std::pair<unsigned long long /*TabModel* */, int /*strip
index*/>>`, one entry per member, indices sequential from the first member's
index (members are contiguous). **Pass a valid (zeroable) 24-byte vector and a
VALID id** — a bad id hits `int3/ud2`. (`SplitTabData::ListTabs` `0x8184fe0`
alone gives just the `vector<TabModel*>` if you don't need indices.)

---

## 4. `SplitTabVisualData` — three ctors; reuse or default-construct

**Struct = 16 bytes: `int layout @+0`, `double ratio @+8`** (4 bytes padding
between). Default ratio `0.5` = `0x3fe0000000000000`. Three overloaded ctors,
all return `rax=this`:

| ctor RVA | ABI | effect |
|---|---|---|
| **`0x2ce031e`** | `(rcx=this)` | `[rcx]=0; [rcx+8]=0.5` → `{layout=0, ratio=0.5}` (default ctor) |
| `0x80dae1e` | `(rcx=this, edx=int layout)` | `[rcx]=edx; [rcx+8]=0.5` → `{layout, 0.5}` |
| `0x6844484` | `(rcx=this, edx=int layout, xmm2=double ratio)` | `[rcx]=edx; [rcx+8]=xmm2` → `{layout, ratio}` |

Disasm of `0x2ce031e` (the one the task pins):
```
0x2ce031e: mov rax,rcx
0x2ce0321: mov dword ptr [rcx], 0                 ; layout = 0
0x2ce0327: movabs rcx, 0x3fe0000000000000
0x2ce0331: mov qword ptr [rax+8], rcx             ; ratio = 0.5
0x2ce0335: ret
```

**Recommendation: reuse the existing split's visual** — copy 16 bytes from
`SplitTabData+8` (§1) — so the grown split keeps its current orientation and
ratio (this is what `UpdateTabInSplitImpl` does). Only construct a fresh one
(`0x80dae1e(&vd, layout)` for a chosen axis, or `0x2ce031e(&vd)` for the default
`{0, 0.5}`) if you intend to reset the layout. (`split-insert.md` previously
cited `0x80dae1e` as "the" ctor; both are ctors — `0x80dae1e` takes `layout` in
`edx`, `0x2ce031e` is the no-arg default. Same struct.)

---

## 5. Pitfalls (each will blank content or crash if wrong)

1. **Never pass an already-split tab to `AddToSplitImpl`.** Every index in `r9`
   is `IsSplit()`-checked and a `true` → `int3/ud2` (`0xa273ec9`→`0xa274403`).
   This is why the remove must come first, and why the *new* tab must also not
   already belong to another split.
2. **Never pass a size-1 member vector.** `CreateSplit`'s `cmp [r8+8],1; jbe`
   (`0x818836d`) silently creates nothing — the grow path must always hand over
   the **full** `{all existing + new}` list (≥2).
3. **Re-resolve indices after the remove.** `RemoveSplitImpl`→`Unsplit` moves the
   members back out as standalone tabs; stale pre-remove indices can point at the
   wrong tab. Hold the `TabModel*`s across the remove and re-run
   `GetIndexOfTabRecursive` (mirrors `0xa273504`+).
4. **`anchor` (arg5) must be in range `[0, tabCount)`** or `0xa273ef2`/`0xa273f08`
   hit `int3/ud2`. Use the smallest re-resolved member index.
5. **`SplitIndexVec` buffer ownership.** The `int*` at `+0` must come from Edge's
   `operator new` (`0x36b3c20`); `AddToNewSplit` builds it that way (and sorts +
   dedups in place). `AddToSplitImpl` reads `data`/`size` and may re-sort; treat
   the buffer as owned by the vector you pass (free it yourself after the call,
   the same allocator). It does **not** take ownership of your `TabModel*`s.
6. **`rdx` is an out-`base::Token`, not scratch** — the tail writes the resulting
   id there (`0xa274365`). Give it a real 16-byte buffer; reading it back is a
   free confirmation the split re-formed under your id.
7. **Don't use `AddToNewSplit` to grow** — it caps at 2 (`0xa273988`) and mints a
   fresh id via `CreateRandom`, losing the split's identity (fires a *new-split*
   observer cascade instead of preserving the grown one).
8. **View-side container must exist before the feed reaches index 2.** The data
   model drives `BrowserView::ShowSplitView`'s `SetWebContentsAtIndex(mcv, wc, 2)`
   call; without the DLL's `containers_[2]` + `[MCV+0x370]=3` and the
   `index<2` guard neutralized, pane 2 stays blank (see `split-insert.md` §4,
   `tab-wiring.md`). That is orthogonal to the model calls above but required for
   the third pane to render.
9. **The added tab should not be the active tab** (it is a background tab here);
   the active tab is already a member. Adding a tab that is already in *this*
   split is also a crash (pitfall 1).

---

## Proven vs. inferred

**Proven by disasm (quoted above):**
- Token = 16 bytes; `SplitTabId` lives at `TabModel+0x68`/`+0x78`; `GetSplit`
  (`vtable+0x118`) and `IsSplit` (`vtable+0x108`, `0x2eeb530`) semantics.
- `AddToSplitImpl` full reg/stack signature incl. `rdx` as out-Token, `r9`
  looped in full with no cap, the per-index `IsSplit` `ud2`, and the stack-arg
  offsets (`[rsp+0x3d0/0x3d8/0x3e0]`).
- `CreateSplit`'s only guard = `size>1`; path (a)/(a′) failure modes.
- The remove+rebuild+add transaction and its flag/visual/anchor values
  (from `UpdateTabInSplitImpl` and `AddToNewSplit`).
- `GetTabsAndIndicesInSplit`, `GetSplitData`, `GetSplitForTab` signatures; three
  `SplitTabVisualData` ctor shapes.

**Inferred / not run at runtime:**
- `flag` (arg7) meaning: `0`=user-new, `1`=transactional rebuild (from the two
  call sites); the exact branch it selects inside `NotifySplitTabCreated` was not
  traced. Growth should use `1` (matches `UpdateTabInSplitImpl`).
- Whether `RemoveSplitImpl`→`Unsplit` *actually* shifts the member strip indices
  in practice (it may leave them in place). The code re-resolves regardless; this
  guidance follows Edge's own defensive re-resolve rather than assuming.
- `rdx`/arg layout reconfirmed statically from two callers; not executed. This
  drives crash-sensitive C++ — single-step the first call under a debugger before
  trusting it in the wild.
- `GetTabsAndIndicesInSplit`'s `pair.first` is typed `unsigned long long` in the
  PDB (the emplace symbol); it is the `TabModel*` reinterpreted. Treat as pointer.
```
