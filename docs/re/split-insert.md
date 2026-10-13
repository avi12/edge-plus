# Growing a split by one member — native insert primitives (`AddToSplitImpl` & friends)

Reverse-engineered from the **public PDB** for `msedge.dll` 155.0.4283.45
(image base `0x180000000`), via `tools/re/{symbols,disasm}.py` + scratch
`drange.py`/`xref.py`. Every `0x…` is an **RVA** unless written `0x18…` (linear,
as the disassembler prints call targets). `linear = 0x180000000 + rva`. Re-verify
after an Edge update. Builds on `controller.md`, `tab-wiring.md`,
`split-view-abi.md`, `split-menu.md`.

> **The question.** We can already build N view containers and tile them. What is
> the cleanest way for the injected DLL to **add a tab to an existing split** so
> the `SplitTabData` becomes a genuine N-member split that Edge's own lifecycle
> feeds (via `BrowserView::ShowSplitView`) and tears down (via
> `RemoveSplit`/`Unsplit`) — driving directional "add pane" through native
> primitives instead of bolting panes on?

> **Bottom line.** There is **no "append one tab to a live split" entry point**.
> The native model only ever *builds* a split from a full member list, through one
> N-generic worker: **`TabStripModel::AddToSplitImpl` (`0xa273e30`)** → it
> re-creates a `SplitTabCollection` from a `std::vector<int>` of tab indices +
> a split id + a `SplitTabVisualData`. `AddToNewSplit` is a thin wrapper that
> **caps at 2** and makes a *new* split; `AddToSplitImpl` itself has **no upper
> cap**. The lifecycle-correct way to grow a 2-split to 3 is the exact transaction
> `UpdateTabInSplitImpl` already performs: capture the split id + visual data,
> `RemoveSplitImpl(id, flag=1)`, rebuild the index vector with the extra member,
> `AddToSplitImpl(…, sameId, vecN, …)`. Its tail fires `NotifySplitTabCreated`,
> which re-runs `ShowSplitView`, whose loop (bounded by `ListTabs().size()`) now
> feeds index 2. Teardown (`RemoveSplit` → `Unsplit`) is fully N-generic and
> returns **all** members to standalone tabs. The only DLL obligation on teardown
> is to shrink its own extra MCV containers — native `CloseSplitView` only touches
> container[1].

---

## 1. `AddToNewSplit` vs `AddToSplitImpl` — signatures, cap, new-vs-existing

### `TabStripModel::AddToNewSplit` — `0xa273926` (creates a NEW split, **caps at 2**)

ABI (`__fastcall`):

| reg | type | meaning |
|---|---|---|
| rcx | `TabStripModel*` | this (`+0x108` is a reentrancy guard byte it sets) |
| rdx | `base::Token*` | **out** — receives a freshly minted split id |
| r8  | `std::vector<int>*` | tab **indices** to put in the split (`[r8]`=data, `[r8+8]`=size) |
| r9  | `split_tabs::SplitTabVisualData*` | `{int layout @+0, double ratio @+8}` (orientation + split ratio) |

**The 2-cap** (raw bytes at `0xa273980`: `48 8b 4b 08 48 8d 41 ff 48 83 f8 02 0f 83 7e 02 00 00`):

```
0xa273980: mov  rcx, [rbx+8]      ; rcx = indices.size()
0xa273984: lea  rax, [rcx-1]
0xa273988: cmp  rax, 2            ; immediate 0x02 at RVA 0xa273988
0xa27398c: jae  0x18a273c10       ; bail  -> size-1 >= 2  (also catches size==0: 0-1 wraps huge)
```

So the accepted sizes are **exactly 1 or 2**. If `size==1` (`cmp rcx,1 / jne`), it
resolves the active tab (`[r15+0x98]` → `GetIndexOfTabRecursive`) and
`vector<int>::insert`s it, making 2. Then:

```
0xa273a72: call base::Token::CreateRandom          ; rdx = brand-new split id
0xa273aa8: call TabStripModel::AddToSplitImpl       ; hand off (r8=random id, r9=indices, +visual)
```

**`AddToNewSplit` is the "make a new 2-pane split" primitive. It cannot grow a
split past 2** (short of patching the `0x02` at `0xa273988`), and it always mints a
fresh id. Not the growth path.

### `TabStripModel::AddToSplitImpl` — `0xa273e30` (the N-generic worker)

ABI (`__fastcall`, 4 reg + 3 stack args; derived from both call sites):

| reg / stack | type | meaning |
|---|---|---|
| rcx | `TabStripModel*` | this |
| rdx | 16-byte region ptr | caller scratch/out (`mov [rsp+0x38],rdx`; both callers pass a stack temp). Not load-bearing for us — pass a valid 16-byte buffer. |
| r8  | `base::Token*` | **split id** — copied into the `CreateSplit` call (`[rsp+0x320]`). Caller chooses it: random (new) or an existing split's id (preserve identity). |
| r9  | `std::vector<int>*` | **member tab indices** (`[r9]`=data, `[r9+8]`=size). **Looped in full — no cap.** |
| `[rsp+0x20]` (arg5) | `int` | anchor / insertion index |
| `[rsp+0x28]` (arg6) | `SplitTabVisualData*` | orientation + ratio → becomes `CreateSplit`'s r9 |
| `[rsp+0x30]` (arg7) | `int` | flag (0 from `AddToNewSplit`, 1 from `UpdateTabInSplit`) |

Body (no `cmp …,2` anywhere):

```
0xa273e82: r13 = [r9+8] << 2                 ; loop bound = size*4, ALL indices
0xa273ea3:   GetTabAtIndexRecursive(idx)     ; int index -> TabInterface*
0xa273ec1:   tab->vtbl[0x108]()              ; "is already in a split?" -> if yes, ud2 (0xa274403)
0xa273ed5:   emplace_back<TabInterface*>     ; build vector<TabInterface*>
0xa273f77: call MoveTabsAndSetPropertiesImpl ; make the members contiguous in the strip
0xa273fad: call tabs::TabStripCollection::CreateSplit(coll=[this+0x10], id, tabVec, visual)
            ; CreateSplit (0x818834e): guard size>1, NO upper bound (controller.md)
…observer cascade…
0xa27435d: call TabStripModel::NotifySplitTabCreated   ; <-- fires the VIEW feeder
```

Tail fires `CompleteModelUpdateTransaction`, `TabStripModelChange`/`OnChange`,
then **`NotifySplitTabCreated` (`0xa276ec6`)**. The only `ud2`/`int3` sites
(`0xa274403`/`0xa274406`) are the "tab already in a split" and index-bounds
bailouts — **not** a member-count cap.

**Callers** (`xref.py` on `0xa273e30`):

| call site | caller | role |
|---|---|---|
| `0xa273aa8` | `AddToNewSplit+0x182` | new split (capped at 2 upstream) |
| `0xa2736d3` | `UpdateTabInSplitImpl+0x6d3` | **mutate an existing split's membership** |
| `0xa2744d1` | `RestoreSplit+0xc7` | restore a closed split |

**Verdict:** `AddToNewSplit` = create-new, cap 2. `AddToSplitImpl` = the shared,
**uncapped** re-create-from-member-list worker. Growth past 2 goes through
`AddToSplitImpl` (directly, or via `UpdateTabInSplit`).

### `UpdateTabInSplit` — `0xa272ccc` → `UpdateTabInSplitImpl` — `0xa273000`

This is the native "change which tabs are in an existing split" transaction, and
it is the template for growth. `UpdateTabInSplit` ABI:
`(rcx=TabStripModel*, rdx=TabInterface* tab, r8d=int index, r9d=int flag)` → calls
`UpdateTabInSplitImpl` with the same args. The impl:

```
0xa2730b2: GetSplitTabCollection([this+0x10], id)      ; find the live split
0xa273084: save split id         -> [rsp+0x120]        ; *** preserve identity ***
0xa273110: save SplitTabData+8   -> [rsp+0x80]         ; *** preserve {layout,ratio} ***
0xa273272: r8d=1
0xa273278: call RemoveSplitImpl(this, &id, 1)          ; tear down the old collection
0xa27345c/0xa2734d2: MoveTabToIndexImpl                ; keep tabs contiguous
0xa273504: loop ListTabs members -> emplace_back<int>  ; rebuild index vector
0xa273657: vector<int>::insert(active-tab index)       ; add [this+0x98] into the set
0xa2736d3: call AddToSplitImpl(this, scratch, &SAME_id, vecN, anchor, &SAME_visual, 1)
```

So the transaction is: **preserve id + visual → `RemoveSplitImpl(id,1)` → rebuild
member index vector → `AddToSplitImpl` under the same id.** That is precisely
"grow by one member while keeping the split's identity, orientation and ratio."
(Caveat: the member it inserts is the **active** tab `[this+0x98]`; to add a
specific background tab deterministically, activate it first or replicate the
steps with your own index vector — see §6.)

---

## 2. How the added tab is identified — index vector; existing OR new tab

Membership is always a **`std::vector<int>` of strip indices**, resolved to
`TabInterface*` by `tabs::TabCollection::GetTabAtIndexRecursive` inside
`AddToSplitImpl`. So:

- **Add an existing background tab:** put its strip index in the vector. No new
  tab is created. (`GetTabsAndIndicesInSplit` (`0xa269a50`) enumerates the current
  members as `vector<pair<TabInterface*,int>>` — use it to read the existing
  indices, then append the new one.)
- **Add a brand-new tab (the "pick/open a tab for the new pane" surface):** the
  controller does this in
  `SplitWindowControllerImplV2::AddSplitTabAtActiveTab` (`0xa020550`):

  ```
  0xa0205e7: InsertDetachedTabAt(tsm, &detachedTab, activeIdx+1, /*r9d*/1)  ; open/insert the new tab
  0xa020601: SplitTabVisualData::SplitTabVisualData(&vd, layout)            ; orientation
  0xa02060b: operator new(4); [vec]=activeIdx                               ; 1-element index vector
  0xa020639: AddToNewSplit(tsm, &newId, &vec, &vd)                          ; -> 2-member split
  ```

  i.e. it funnels through `AddToNewSplit` (hence native "new pane" is always 2).
  `AddWebContentsAtActiveTab` (`0xa020360`) is the sibling that wraps a raw
  `WebContents` into a tab first. For N>2 we reuse `InsertDetachedTabAt` /
  `TabStripModel::GetWebContentsAt` to source the tab, but feed the index into
  `AddToSplitImpl`, not `AddToNewSplit`.

`SplitTabVisualData` (ctor `0x80dae1e`) is 16 bytes: `{int layout @+0; double
ratio @+8 (default 0.5 = 0x3fe0000000000000)}`. **`layout` (field 0) is the
orientation enum** — this is where "vertical vs horizontal split" comes from. The
left/right-vs-top/bottom *direction* is the combination of this axis and the
**order of indices** in the member vector (plus `UpdateSplitLayout`, below).

`TabStripModel::UpdateSplitLayout` (`0xa2727ba`), ABI
`(rcx=this, rdx=SplitTabId*, r8d=int layout, r9=…)`, flips orientation on a live
split: `GetSplitTabCollection` → `[coll+0x90]` = `SplitTabData`, compares
`[SplitTabData+8]` (current layout) to `r8d`, updates + notifies if changed.

---

## 3. The feeder re-runs and reaches index 2

`AddToSplitImpl`'s tail `NotifySplitTabCreated` drives the `TabStripModelObserver`
cascade → **`BrowserView::OnSplitTabChanged` (`0xc93b190`) → `ShowSplitView`
(`0xc93594a`)** (controller.md §0). `ShowSplitView`'s feed loop is bounded by
`SplitTabData::ListTabs()`-count, not a constant (tab-wiring.md §3):

```
0xc935a1a: call SplitTabData::ListTabs     ; fills the tab vector
0xc935a1f: rax = [rdi+8]                    ; COUNT (== 3 after we grow)
0xc935a23: r13 = count*8                    ; loop bound
0xc935b4b: rcx = tabs[i]
0xc935b69: r8d = i                          ; index passes straight through
0xc935b6c: call MultiContentsView::SetWebContentsAtIndex(mcv, wc, i)   ; i = 0,1,2,…
```

So once `ListTabs()` returns 3, the feeder **does** call
`SetWebContentsAtIndex(mcv, wc, 2)` — hitting the `index < 2` guard
(`0xc930f22`) we plan to neutralize. Confirmed: **grow the data model → the native
feeder populates pane 2 on its own.** (`SetWebContentsAtIndex` is the *only*
caller of itself's path; single xref from `ShowSplitView+0x222`.)

---

## 4. Container growth timing — build container[2] before the feed reads it

`SetWebContentsAtIndex` (`0xc930f18`) reads the container **after** both guards
(tab-wiring.md §1):

```
0xc930f1f: cmp r8d, 2        ; guard A (index<2)  — patch 0x02@0xc930f22 -> 0xFF
0xc930f23: jae ud2
0xc930f32: cmp [rcx+0x370], rax ; guard B (index < count)
0xc930f42: mov rcx, [rdi+0x368]   ; containers.begin()   <-- read happens HERE
0xc930f49: mov rax, [rcx+rax*8]   ; containers[index]
0xc930f4d: mov rcx, [rax+0x3a8]   ; container->ContentsWebView*
0xc930f54: call ContentsWebView::SetWebContents
```

Because the vector dereference (`[MCV+0x368][index]`) is **after** the guards,
the DLL's `SetWebContentsAtIndex` hook can safely:

1. if `index >= [MCV+0x370]` (count): build container[index] (replicate the
   `EdgeContentsContainerView` ctor from `ctor.md`, parent it, `emplace_back` into
   the `+0x368` vector, bump `+0x370`), **then**
2. proceed into the original body.

Guard A still rejects `index>=2`, so also patch `0xc930f22` `0x02`→`0xFF` (or NOP
the `jae`). With the container present and count bumped, guards A+B pass and the
original body wires `container[2]->[+0x3a8]` correctly. **Building before the call
is sufficient** — nothing is cached across the guards.
(Offsets: MCV `+0x368` begin, `+0x370` count, `+0x3f0` resize area, `+0x420`
active-pane index, `+0x428` `{layout,ratio}`; container `+0x3a8` ContentsWebView.)

---

## 5. Teardown is N-generic; the DLL must shrink only its own containers

### Data side — returns **all** members to standalone tabs

`TabStripModel::RemoveSplit` (`0xa275c18`), ABI `(rcx=this, rdx=SplitTabId*)`:

```
0xa275c70: NotifyInactiveSplitTabWillBecomeHidden
0xa275c86: RemoveSplitImpl(this, &id, /*r8d*/0)      ; flag 0 = user unsplit
0xa275c8e: CompleteModelUpdateTransaction
```

`RemoveSplitImpl` (`0xa26df6a`), ABI `(rcx=this, rdx=SplitTabId*, r8d=flag)`:

```
0xa26dfc0: GetTabsAndIndicesInSplit(&members, &id)
0xa26dfd8: tabs::TabStripCollection::Unsplit([this+0x10], &id)   ; the real teardown
…selection bookkeeping… CompleteModelUpdateTransaction… NotifySplitTabRemoved
```

`tabs::TabStripCollection::Unsplit` (`0x8188592`) is fully N-generic:

```
0x818866e: GetTabsRecursive(&allTabs)       ; ALL members of the split collection
0x8188677: r13 = count*8                     ; loop over every member
0x81886a7:   MoveTabImpl(tab -> strip at r12++) ; move each back out as a standalone tab
0x81886c6: RemoveTabCollectionImpl(emptySplitColl)
0x81886e5: delete the now-empty SplitTabCollection
```

So **`RemoveSplit`/`Unsplit` on a 3-member split returns all 3 tabs to normal
standalone tabs** (none are closed). `flag` differs: `RemoveSplit` passes 0
(user unsplit); `UpdateTabInSplitImpl` passes 1 (transactional remove inside a
rebuild). Collapse/eject therefore works "for free" at the model level for any N.

### View side — native only cleans container[1]

When the split disappears, `OnSplitTabChanged` → `ShowSplitView` sees no split and
calls `MultiContentsView::CloseSplitView` (`0xc93117a`). It:

```
0xc931182: if resize-area[+0x3f0] not visible: return        ; no-op when already un-split
0xc931198: if active-idx [+0x420] != 0: SwapContentsInSplitView  ; move active to slot 0
0xc9311b7: container[1]->[+0x3a8] = ContentsWebView; SetWebContents(NULL)  ; *** index 1 only ***
0xc9311fb: resize-area[+0x3f0].SetVisible(false)
0xc931209: UpdateContentsBorderAndOverlay
```

**It hard-codes index 1 and never shrinks the vector or changes `[+0x370]`
(count).** So after teardown the MCV still physically holds 3 containers;
container[2] keeps a stale `WebContents` and its tiled geometry. Since our N-tiling
detour triggers on `count > 2`, it would keep tiling 3 panes for a split that no
longer exists.

**DLL teardown obligation (the only one):** on the no-split transition (hook
`CloseSplitView`, or detect `ListTabs()`-count dropping), for every container the
DLL added (index ≥ 2): `ContentsWebView::SetWebContents(cwv, NULL)`, remove it from
the `+0x368` vector, and set `[MCV+0x370]` back to 2 (or to the current
member count). Dropping count to ≤ 2 makes the geometry detour defer to the native
2-pane / un-split path automatically (it is gated on `count > 2`), so **no
geometry cleanup is needed beyond fixing the count.** Member-count → container-count
is the DLL's invariant to maintain: native keeps `[MCV+0x370]` in sync only for
the 2-pane case; for N the DLL owns it on both grow and shrink.

---

## 6. Recommendation — "add a pane in direction D"

Drive it through the native data model so the feeder wires the pane and
`RemoveSplit`/`Unsplit` tears it down. Two hooks already planned
(`SplitTabMenuModel` ctor + `ExecuteCommand`, split-menu.md) call this on the UI
thread:

**Preferred — replicate the `UpdateTabInSplitImpl` transaction (full control over
which tab and where):**

1. `GetSplitTabId(model,&id)` (`0xa265776`) → the active split's id;
   `GetSplitData` (`0xa2677a4`) → `SplitTabData*`; read `{layout,ratio}` from
   `SplitTabData+8`.
2. `GetTabsAndIndicesInSplit(tsm,&members,&id)` (`0xa269a50`) → current member
   indices.
3. Pick the new member's **strip index**: an existing background tab
   (`GetWebContentsAt`/its index), or open one with `InsertDetachedTabAt`
   (`0xa267bb6`) adjacent to the split. Build `vecN` = existing indices + new
   index (sorted, contiguous).
4. `RemoveSplitImpl(tsm,&id,/*flag*/1)` (`0xa26df6a`) — tear down the old
   collection (tabs stay in the strip).
5. `AddToSplitImpl(tsm, &scratch16, &id, &vecN, anchorIdx, &visual, /*flag*/1)`
   (`0xa273e30`) — re-create under the **same id**. Set `visual.layout` for the
   axis of direction D (vertical ⇒ left/right, horizontal ⇒ top/bottom); place the
   new index before/after the others for the near/far side. Optionally
   `UpdateSplitLayout` (`0xa2727ba`) afterwards to flip orientation.
6. `AddToSplitImpl`'s `NotifySplitTabCreated` → `OnSplitTabChanged` →
   `ShowSplitView` feeds indices 0..N-1. Our `SetWebContentsAtIndex` hook
   (§4) builds container[2] before the feed reads it; the N-tiling geometry detour
   (`count > 2`) lays it out.

**Simplest native call (if its active-tab-insertion semantics suit the menu):**
`TabStripModel::UpdateTabInSplit(tsm, tab, index, flag)` (`0xa272ccc`) performs
steps 4+5 internally and preserves id/visual — but it inserts the **active** tab
(`[tsm+0x98]`), so activate the intended tab first, or use the explicit path above
for a specific background tab. Confirm the add-vs-move semantics at runtime before
relying on it.

**Do NOT use `AddToNewSplit` for growth** — it caps at 2 (`0xa273988`) and mints a
new id (loses the split's identity, fires a fresh create instead of an update).

**Teardown:** nothing extra on the model side — `RemoveSplit`/`Unsplit` already
eject all N members as standalone tabs. The DLL's only job is to drop its extra
MCV containers and reset `[MCV+0x370]` on the no-split transition (§5).

---

## Quick RVA reference (this build)

| symbol | RVA | ABI |
|---|---|---|
| `TabStripModel::AddToNewSplit` | `0xa273926` | rcx=this, rdx=out `Token*` id, r8=`vector<int>*` indices, r9=`SplitTabVisualData*` · **cap 1–2** @ `0xa273988` |
| `TabStripModel::AddToSplitImpl` | `0xa273e30` | rcx=this, rdx=scratch16, r8=`Token*` id, r9=`vector<int>*` indices, [+0x20]=int anchor, [+0x28]=`SplitTabVisualData*`, [+0x30]=int flag · **no cap** |
| `TabStripModel::UpdateTabInSplit` | `0xa272ccc` | rcx=this, rdx=`TabInterface*`, r8d=int index, r9d=int flag |
| `TabStripModel::UpdateTabInSplitImpl` | `0xa273000` | same; RemoveSplitImpl(1)+rebuild+AddToSplitImpl(sameId) |
| `TabStripModel::UpdateSplitLayout` | `0xa2727ba` | rcx=this, rdx=`SplitTabId*`, r8d=int layout |
| `TabStripModel::GetTabsAndIndicesInSplit` | `0xa269a50` | rcx=this, rdx=out `vector<pair<Tab*,int>>`, r8=`SplitTabId*` |
| `TabStripModel::RemoveSplit` | `0xa275c18` | rcx=this, rdx=`SplitTabId*` (flag 0) |
| `TabStripModel::RemoveSplitImpl` | `0xa26df6a` | rcx=this, rdx=`SplitTabId*`, r8d=int flag |
| `tabs::TabStripCollection::Unsplit` | `0x8188592` | rcx=TabStripCollection, rdx=`SplitTabId*` · N-generic |
| `tabs::TabStripCollection::CreateSplit` | `0x818834e` | rcx=coll, rdx=`Token`, r8=`vector<Tab*>`, r9=visual · guard size>1 |
| `tabs::TabStripCollection::GetSplitTabCollection` | `0x81882ea` | rcx=coll, rdx=out, r8=`SplitTabId*` |
| `split_tabs::SplitTabVisualData::SplitTabVisualData` | `0x80dae1e` | rcx=this, edx=int layout → `{layout@0, 0.5@8}` |
| `SplitWindowControllerImplV2::AddSplitTabAtActiveTab` | `0xa020550` | new-tab-beside-active → AddToNewSplit |
| `SplitWindowControllerImplV2::AddWebContentsAtActiveTab` | `0xa020360` | wrap WebContents → new split |
| `TabStripModel::InsertDetachedTabAt` | `0xa267bb6` | insert a detached tab at an index |
| `TabStripModel::GetWebContentsAt` | `0x101530e` | rcx=this, edx=index |
| `BrowserView::ShowSplitView` (feeder) | `0xc93594a` | loops `ListTabs()`, SetWebContentsAtIndex(i) |
| `MultiContentsView::SetWebContentsAtIndex` | `0xc930f18` | rcx=MCV, rdx=WebContents*, r8d=index · guard `0xc930f22` |
| `MultiContentsView::CloseSplitView` | `0xc93117a` | nulls container[1] only; leaves count/vector |
| `split_tabs::SplitTabData::ListTabs` | `0x8184fe0` | returns all members |

Key offsets: TabStripModel `+0x10`=root TabStripCollection, `+0x98`=active tab,
`+0x108`=reentrancy guard byte; SplitTabCollection `+0x90`=SplitTabData;
SplitTabData `+8`={int layout, double ratio}; MCV `+0x368`=containers.begin,
`+0x370`=count, `+0x3f0`=resize area, `+0x420`=active-pane index, `+0x428`=visual;
container `+0x3a8`=ContentsWebView.

## Honest uncertainties

- `AddToSplitImpl` arg2 (rdx, `[rsp+0x38]`): both callers pass a stack temp and it
  is only re-read near the dtors — treated here as caller scratch/out. Not proven
  to carry meaning; pass a valid 16-byte buffer.
- `UpdateTabInSplitImpl` inserts the **active** tab (`[tsm+0x98]`); the exact
  trigger semantics (add vs reposition, and the role of `index`/`flag`) were read
  statically, not confirmed at runtime. The explicit RemoveSplitImpl+AddToSplitImpl
  path in §6 avoids depending on them.
- `flag` (AddToSplitImpl +0x30 / RemoveSplitImpl r8d): 0 for user new/unsplit, 1
  for the transactional rebuild — inferred from the two call sites; the precise
  branch it selects inside was not fully traced.
- Middle-pane controller parity (toolbar/input/focus) still assumes first+last
  only (`OnSplitTabAdded`, controller.md §4) — unchanged by this analysis.
