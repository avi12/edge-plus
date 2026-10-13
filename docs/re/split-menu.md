# Split 3-dot menu (`SplitTabMenuModel`) — how to add "Add pane above/below/left/right"

Target: `msedge.dll` 155.0.4283.45, image base `0x180000000`.
`linear = 0x180000000 + rva`. All addresses below are **RVAs** unless written as `0x18…` (linear, as the disassembler prints call targets).

Bottom line: the split 3-dot menu is a **native `ui::SimpleMenuModel`** (`SplitTabMenuModel`), shown through `views::MenuModelAdapter` + `views::MenuRunner` in every path (native toolbar button *and* the WebUI control). So we extend it the cheap way: **hook the ctor to append 4 items, hook `ExecuteCommand` to dispatch them.** Nothing WebUI-side is required, and no label/icon/enabled plumbing is required.

---

## 1. Base class & item storage

`SplitTabMenuModel::SplitTabMenuModel` @ **`0xa264f8e`** (full body `0xa264f8e … 0xa265310`, cold `~` thunk tail `0xa2653b4`).

- **Base ctor:** `ui::SimpleMenuModel::SimpleMenuModel` @ `0x1826a02f2` (RVA `0x26a02f2`), called at `0xa264fd7`.
- The class is **its own `ui::SimpleMenuModel::Delegate`**: there are two vtables and two subobjects.
  - primary `ui::SimpleMenuModel` vtable at object **+0x00** → `SplitTabMenuModel::`vftable`` `0x112467e0`
  - delegate `ui::SimpleMenuModel::Delegate` vtable at object **+0x48** → second `SplitTabMenuModel::`vftable`` `0x11246950`
  - ctor passes `delegate = this+0x48` to the base ctor (`lea rdx,[rcx+0x48]` @ `0xa264fc8`).

### Object layout (SplitTabMenuModel, size ~0x68)
| offset | meaning | evidence |
|---|---|---|
| +0x00 | `ui::SimpleMenuModel` vtable | ctor `0xa264fc5` |
| +0x10 | items array **begin** ptr | `IsEnabledAt` `mov rax,[rcx+0x10]` `0x342b6c7` |
| +0x18 | item **count** (`GetItemCount` returns this) | `GetItemCount` `0x28cf330` = `mov rax,[rcx+0x18];ret`; `IsEnabledAt` bound check `0x342b6b7` |
| +0x28 | delegate ptr (== this+0x48) | `IsEnabledAt` `mov rcx,[rsi+0x28]` `0x342b6da` |
| +0x48 | `Delegate` vtable | ctor `0xa264fd3` |
| +0x50 | `raw_ptr<TabStripModel>` (ctor arg 2) | ctor `mov [rsi+0x50],rdi` `0xa265001`; used as `this` for every `TabStripModel::…` call |
| +0x58 | `int` menu **source/type** (ctor arg 3) ∈ {0,1,2} | ctor `mov [rsi+0x58],ebx` `0xa265005`; branch `cmp ebx,1 / test ebx,~2` |
| +0x5c | `int` optional-tab-index **value** | ctor `mov [rsi+0x5c],r9`; `GetSplitTabId` `mov edx,[rcx+0x5c]` `0xa265799` |
| +0x60 | `bool` optional **has_value** | `GetSplitTabId` `cmp byte[rcx+0x60],1` `0xa26578f`; `ExecuteCommand` `cmp byte[rbx+0x18],0` (rbx=delegate=this+0x48) |

### Each `SimpleMenuModel` item = **0x148 bytes** (`IsEnabledAt` `imul rbx,rdx,0x148` @ `0x342b6cb`)
- item +0x00 : type (int)
- item +0xec : per-item **enabled** byte (`IsEnabledAt` returns `[items + idx*0x148 + 0xec]` when delegate not consulted)

### Item population calls in the ctor (ctor arg3 `ebx` selects the set)
Default construction everywhere of interest passes `ebx = 0` and optional disengaged (see §4), which gives the **full menu with feedback**.

| order | call site | API | cmd id | string id / icon |
|---|---|---|---|---|
| 1 | `0xa26507d` | `AddItemWithStringIdAndIcon` | **0x6a9** | str `0x3a40`, "open-in-full"/exit icon |
| 2 | `0xa26508f` | `AddSeparator` | — | |
| 3 | `0xa2650c8` (ebx∈{0,2}) | `AddItem` (empty label ⇒ dynamic) | **0x6a7** | dynamic |
| 4 | `0xa265102` (ebx∈{0,2}) | `AddItem` | **0x6a8** | dynamic |
| 5 | `0xa265125` | `AddSeparator` | — | |
| 3' | `0xa2651f5` (ebx==1 only) | `AddItemWithStringIdAndIcon` | **0x6a6** | str `0x3a3b`, `kCloseIcon` |
| 6 | `0xa265256` (common tail) | `AddItem` | **0x6a5** | dynamic; element-id `kReversePositionMenuItemProvider` |
| 7 | `0xa265383` (ebx==0 only, if `chrome::CanShowFeedback`) | `AddItemWithStringIdAndIcon` | **0x6aa** | str `0x3a49`, feedback icon |

Element identifiers set via `SetElementIdentifierAt` (`0x18b65df14`) after `GetIndexOfCommandId` (`0x18b65dc84`): `kReversePositionMenuItemProvider` `0x1263b0e8`, `kExitSplitMenuItemProvider` `0x1263b1b0`.

Helper symbols for appending: `AddItem` `0x101bf04`, `AddItemWithStringId` `0x101bdce`, `AddItemWithIcon` `0x2b5ea80`, `AddItemWithStringIdAndIcon` `0x2b5ea12`, `AddSeparator` `0x317665a`, `InsertItemAt` `0xb65d5c2`, `InsertItemWithIconAt` `0xb65e9a2`, `GetIndexOfCommandId` `0xb65dc84`, `GetItemCount` `0x28cf330`.

**Signatures** (from call-site register usage):
- `AddItem(this, int command_id, const std::u16string& label)` — `rcx,edx,r8`. (ctor builds a 24-byte zeroed `u16string` ⇒ empty label ⇒ item is rendered via the dynamic-label path.)
- `AddItemWithIcon(this, int command_id, const std::u16string& label, const ui::ImageModel& icon)`.
- `AddItemWithStringIdAndIcon(this, int command_id, int string_id, const ui::ImageModel& icon)` — ctor `rcx,edx=cmd,r8d=string_id,r9=&icon` @ `0xa26507d`.

---

## 2. Command id space

The model's command ids are a **dense local enum `0x6a5 … 0x6ab` (1701 … 1707)**. Index into every dispatch table = `command_id - 0x6a5`.

| cmd | dec | item meaning | dynamic? | label str | dynamic icon case |
|---|---|---|---|---|---|
| 0x6a5 | 1701 | **Swap / reverse panes** (`ReverseTabsInSplit`) | yes | `0x3a3a` | swap L-R / up-down |
| 0x6a6 | 1702 | **Close this (active) pane** | no (static) | `0x3a3b` | — (static `kCloseIcon`) |
| 0x6a7 | 1703 | **Close pane 1 (start/left)** | yes | `0x3a3d`/`0x3a3f` | `kEdgeSmtcClosePanelStart` |
| 0x6a8 | 1704 | **Close pane 2 (end/right)** | yes | `0x3a3c`/`0x3a3e` | `kEdgeSmtcClosePanelEnd` |
| 0x6a9 | 1705 | **Exit split** (`RemoveSplit`) | no | `0x3a40` | — (static) |
| 0x6aa | 1706 | **Send feedback** (`SendFeedback`) | no | `0x3a49` | — (static) |
| 0x6ab | 1707 | defined but **not added** by ctor (dyn label `0x3a47`/`0x3a48`, handler `0xa265c3e`) | yes | `0x3a47/8` | split-scene |

`IsItemForCommandIdDynamic` @ `0xa265410` encodes the dynamic set as bitmask `0x4d >> (cmd-0x6a5)` for `(cmd-0x6a5) < 7`: bits {0,2,3,6} ⇒ dynamic for `0x6a5,0x6a7,0x6a8,0x6ab`.

**Safe range for 4 new ids: `0x6ac … 0x6af` (1708 … 1711).** They are immediately past the enum and used by nothing:
- `IsItemForCommandIdDynamic(0x6ac)` → `(0x6ac-0x6a5)=7`, `cmp 7,7` not-below ⇒ returns **false** (our items are static-label, good).
- `GetLabelForCommandId` / `GetIconForCommandId` / `ExecuteCommand` jump tables cover only index 0..6 and fall through to default/`ja` for anything else.
- MenuModelAdapter routes selections purely by the model's own command ids, so adjacency is collision-free. (If you want extra distance from any global command routing, `0xC100…0xC103` works identically; adjacency is not actually required.)

---

## 3. `ExecuteCommand` dispatch

`SplitTabMenuModel::ExecuteCommand` @ **`0xa265830`**. Signature `ExecuteCommand(Delegate* this /*rcx = model+0x48*/, int command_id /*edx*/, int event_flags /*r8d*/)`. First thing it does: `lea r14,[rcx-0x48]` (`0xa265862`) to recover the **model**.

Control flow:
1. `edi = command_id - 0x6a5`. If `== 5` (cmd `0x6aa`) → `SplitTabMenuModel::SendFeedback(model)` `0xa265cf6`, then UMA/RecordAction, return. (`0xa26586c…`)
2. Otherwise resolve the target split:
   - `GetSplitTabId(model, &splitId)` `0xa2659be` (→ `0xa265776`): uses the active tab (or the index at `model+0x5c` when `model+0x60==1`), checks `tab->vtable[0x108]()` "is in split", returns `tab->vtable[0x118](&out)` split id (`std::optional`, 16 bytes).
   - `TabStripModel::GetSplitData(tabStrip=[model+0x50], splitId)` `0xa2677a4` → `SplitTabData*`.
   - `SplitTabData::ListTabs(&vec)` `0x188184fe0`; asserts `vec.size()==2`.
3. Jump table (base = `kExitSplitMenuItemProvider+0x134` = **`0x1263b2e4`**), indexed by `command_id-0x6a5`:

| idx | cmd | target RVA | action |
|---|---|---|---|
| 0 | 0x6a5 | `0xa265a5a` | `TabStripModel::ReverseTabsInSplit(tabStrip, splitId)` `0xa2737b4` |
| 1 | 0x6a6 | `0xa265a1b` | requires optional engaged (`[model+0x60]`); `GetWebContentsAt(tabStrip, [model+0x5c])` → `CloseWebContents(…,3)` |
| 2 | 0x6a7 | `0xa265a78` | `CloseWebContents(vec[IsRTL?1:0])` |
| 3 | 0x6a8 | `0xa265a33` | `CloseWebContents(vec[IsRTL?0:1])` |
| 4 | 0x6a9 | `0xa265a3c` | `TabStripModel::RemoveSplit(tabStrip, splitId)` `0xa275c18` |
| 5 | 0x6aa | `0xa265ab3` | (dead; feedback handled in step 1) |
| 6 | 0x6ab | `0xa265c3e` | (unused command) |

Tail `0xa265ab3…`: UMA `UmaHistogramExactLinear` + `RecordAction` keyed by the menu-type string table (`kExitSplit+0xb8` ptrs / `+0xd0` lengths, 3 entries), then destroys the temp tab vector and returns.

Call helpers available to our handler: `TabStripModel::GetWebContentsAt` `0x101530e`, `TabStripModel::CloseWebContents` `0x3125d36`, and for **insert**: `TabStripModel::AddToNewSplit` `0xa273926`, `AddToSplitImpl` `0xa273e30`, `UpdateSplitLayout` `0xa2727ba`, `ExecuteAddToNewSplitCommand` `0xa27b56c`, `GetTabsAndIndicesInSplit` `0xa269a50`, `InsertDetachedSplitTabAt` `0xa26a76e`.

---

## 4. Who builds & shows it (native vs WebUI)

`SplitTabMenuModel` ctor callers (direct `E8` xrefs):

| site | caller | arg3 (type) / arg4 (optional) |
|---|---|---|
| `0xa25b217` | `TabMenuModel::Build+0x497` | per-tab context submenu |
| `0xa25cbdb` | `EdgeTabMenuModel::Build+0x4eb` | per-tab context submenu |
| `0xa2c3616` | `SplitTabsToolbarButton::SplitTabsToolbarButton+0x514` | **`xor r8d,r8d; xor r9d,r9d`** ⇒ type 0, optional disengaged |
| `0xa2d30d0` | `WebUISplitTabsControl::HandleContextMenu+0xf0` | **`xor r8d,r8d; xor r9d,r9d`** ⇒ type 0, optional disengaged |

The 3-dot button path = `SplitTabsToolbarButton` (native views button; stores the model at `button+0x908`, `operator new 0x68` @ `0xa2c3602`).

**The popup itself is native views in every path.** `WebUISplitTabsControl::HandleContextMenu` (`0xa2d2fe0`) builds a `SplitTabMenuModel` (stored at `control+0x80`), then `RunMenuAt` (`0xa2d31b2`) constructs `views::MenuModelAdapter(SplitTabMenuModel)` (`0x182b5e8dc` @ `0xa2d3265`) and a `views::MenuRunner` (`control+0x88`; `MenuRunner::Cancel` `0x1854f5310`). So the WebUI control only provides the trigger/anchor — the menu is rendered by the native `SimpleMenuModel`/views stack, exactly like the toolbar button.

**Which split/pane the menu targets:** `GetSplitTabId` resolves it from the **active tab's** split (optional disengaged in both button/WebUI paths); the per-tab context-menu callers can pass an engaged `optional<int>` tab index to target a specific tab (stored at `model+0x5c/+0x60`). Our new handlers get the same split via `GetSplitTabId` + `GetSplitData`.

**Enabled/label/icon machinery (no hooks needed for ours):**
- `IsEnabledAt` `0x342b6b6` consults `delegate->IsCommandIdEnabled` (delegate vtable slot 3, offset +0x18), which is the folded default **`0x26b2ef0` = `mov al,1; ret`** → every command is enabled. The only `SplitTabMenuModel` delegate overrides are slots 7/8/9/10 = `IsItemForCommandIdDynamic` / `GetLabelForCommandId` / `GetIconForCommandId` / `ExecuteCommand`. `IsCommandIdChecked`, `GetAcceleratorForCommandId` are folded trivial defaults too.

---

## 5. Recommendation (injected-DLL extension)

Approach **(a)** — native-menu extension. Two hooks, nothing WebUI-side, no resource/string plumbing.

**Command ids:** `kAddPaneAbove=0x6ac, kAddPaneBelow=0x6ad, kAddPaneLeft=0x6ae, kAddPaneRight=0x6af`.

### Hook A — `SplitTabMenuModel::SplitTabMenuModel` @ `0xa264f8e`
Trampoline: call original, then append our items to the already-built model (`this` = model). Because the toolbar button caches the model and the WebUI path rebuilds it per show, appending in the ctor covers all four construction sites uniformly.

```
orig(this, tab_strip, type, opt);          // build stock menu
size_t at = GetIndexOfCommandId(this, 0x6a9 /*exit split*/); // anchor; always present
// (fall back to GetItemCount(this) if not found)
AddSeparator(this, NORMAL);
AddItem(this, 0x6ac, u"Add pane above");   // or InsertItemAt / AddItemWithIcon
AddItem(this, 0x6ad, u"Add pane below");
AddItem(this, 0x6ae, u"Add pane left");
AddItem(this, 0x6af, u"Add pane right");
```
- `AddItem(this,cmd,&u16string)` → our items are static-label; `IsItemForCommandIdDynamic(0x6ac..)` already returns false, so `GetLabel/GetIconForCommandId` are **never consulted** for them. Hardcode the labels (or feed `AddItemWithStringIdAndIcon` a real `IDS_`/icon if you add resources — optional).
- Items are enabled automatically (folded `IsCommandIdEnabled`→true). To **conditionally** grey one out (e.g. split already full), set the per-item byte `item[idx*0x148 + 0xec] = 0` at build time, using `items_begin = *(void**)(this+0x10)`, `count = *(size_t*)(this+0x18)` — no vtable surgery needed.
- The `std::u16string` arg must be a real libc++ `basic_string` (24 bytes, SSO). Easiest is to build it with the same allocator the DLL uses, or reuse the ctor's pattern (zero then assign). If constructing a libc++ string from the injected module is awkward, add via `AddItemWithStringIdAndIcon` with a stock `IDS_` id instead.

### Hook B — `SplitTabMenuModel::ExecuteCommand` @ `0xa265830`
Trampoline: handle our ids, else tail-call original.

```
void Hook(Delegate* self, int cmd, int flags) {
    if (cmd >= 0x6ac && cmd <= 0x6af) {
        auto* model     = (char*)self - 0x48;
        auto* tab_strip = *(TabStripModel**)(model + 0x50);   // == *(self+8)
        SplitTabId id; GetSplitTabId(model, &id);             // 0xa265776
        SplitTabData* data = TabStripModel__GetSplitData(tab_strip, id); // 0xa2677a4
        Direction d = (cmd==0x6ac)?Up:(cmd==0x6ad)?Down:(cmd==0x6ae)?Left:Right;
        InsertPane(tab_strip, data, d);   // repo native-split entry point
        return;
    }
    orig(self, cmd, flags);
}
```
- `InsertPane` is the repo's N-pane insertion routine (the `native-split` work). The in-DLL primitives it can build on are `TabStripModel::AddToNewSplit` `0xa273926`, `AddToSplitImpl` `0xa273e30`, `UpdateSplitLayout` `0xa2727ba` (takes a layout/orientation arg in `r8d`), and `InsertDetachedSplitTabAt` `0xa26a76e`. Direction ↔ orientation follows the existing location enum used by `GetIconForCommandId`/`split_tabs::GetLastActiveTabLocation` (`0xa23d7fe`): {0 left, 1 right, 2 top/up, 3 bottom/down}.
- Keep the `[model+0x5c]/+0x60` optional in mind: when the menu was opened from a per-tab context menu it targets that specific tab; `GetSplitTabId` already handles both cases, so reuse it rather than re-deriving the active tab.

### Not required
- No hook of `GetLabelForCommandId`, `GetIconForCommandId`, `IsItemForCommandIdDynamic`, `IsCommandIdEnabled`, `MenuRunner`, `MenuModelAdapter`, or any WebUI code. The native menu machinery already renders, enables, and routes arbitrary `SimpleMenuModel` items; we only add items and catch their ids.

### Evidence index (RVAs)
ctor `0xa264f8e`; ExecuteCommand `0xa265830`; GetSplitTabId `0xa265776`; GetLabelForCommandId `0xa265430`; GetIconForCommandId `0xa265550`; IsItemForCommandIdDynamic `0xa265410`; SendFeedback `0xa265cf6`; vtables `0x112467e0` / `0x11246950`; providers `0x1263b0e8` / `0x1263b1b0`; ExecuteCommand jump table `0x1263b2e4`; icon jump table `0x1263b2c8`. Base: SimpleMenuModel ctor `0x26a02f2`, AddItem `0x101bf04`, AddItemWithIcon `0x2b5ea80`, AddItemWithStringIdAndIcon `0x2b5ea12`, AddSeparator `0x317665a`, GetIndexOfCommandId `0xb65dc84`, GetItemCount `0x28cf330`, InsertItemAt `0xb65d5c2`, IsEnabledAt `0x342b6b0`, default IsCommandIdEnabled `0x26b2ef0`. Hosts: SplitTabsToolbarButton ctor call `0xa2c3616`; WebUISplitTabsControl::HandleContextMenu `0xa2d2fe0`, RunMenuAt `0xa2d31b2`. Insert APIs: AddToNewSplit `0xa273926`, AddToSplitImpl `0xa273e30`, UpdateSplitLayout `0xa2727ba`, RemoveSplit `0xa275c18`, ReverseTabsInSplit `0xa2737b4`, GetSplitData `0xa2677a4`, GetWebContentsAt `0x101530e`, CloseWebContents `0x3125d36`.
