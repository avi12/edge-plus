// edge.h — the single binding layer between our DLL and msedge.dll.
//
// SSOT / DRY: every RVA call and every struct-field access goes through here, so
// a struct offset or function signature is written exactly once. offsets.h holds
// the raw numbers; this header gives them names, types and calling conventions.
// On x64 every calling convention collapses to the one Windows ABI, so the
// `__fastcall` tags are documentation, not behaviour.
#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include "offsets.h"
#include "geom.h"
#include "log.h"

namespace fixedge::edge {

using namespace fixedge::edge155;

inline uintptr_t g_base = 0;  // msedge.dll load address, set once by Bind()

inline bool Bind(HMODULE module) {
  g_base = reinterpret_cast<uintptr_t>(module);
  return g_base != 0;
}

inline void* At(uintptr_t rva) {
  return reinterpret_cast<void*>(g_base + rva);
}

// Overwrite one code byte — used to lift the index<2 guard. Returns the byte
// that was there (for the log).
inline uint8_t PatchByte(uintptr_t rva, uint8_t value) {
  void* address = At(rva);
  DWORD oldProtection = 0;
  const uint8_t previousByte = *reinterpret_cast<uint8_t*>(address);
  if (VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtection)) {
    *reinterpret_cast<uint8_t*>(address) = value;
    VirtualProtect(address, 1, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), address, 1);
  }
  return previousByte;
}

inline constexpr uint8_t kNopOpcode = 0x90;  // x86 `nop`, one byte

// Overwrite `byteCount` code bytes with NOPs, copying what was there into
// `savedBytes` (for the log) — used to neutralize a 2-pane-only CHECK so a grown
// split survives.
inline void PatchNop(uintptr_t rva, size_t byteCount, uint8_t* savedBytes) {
  void* address = At(rva);
  DWORD oldProtection = 0;
  auto* code = reinterpret_cast<uint8_t*>(address);
  if (VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE,
                     &oldProtection)) {
    for (size_t i = 0; i < byteCount; i++) {
      savedBytes[i] = code[i];
      code[i] = kNopOpcode;
    }
    VirtualProtect(address, byteCount, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), address, byteCount);
  }
}

// Typed lvalue at object+offset. The one place raw pointer arithmetic on Edge
// objects lives; callers read/write fields by name via the Mcv*/Container*
// helpers below.
template <class T>
inline T& Field(void* object, uintptr_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(object) + offset);
}

// The function in `object`'s vtable at `slotOffset`. offsets.h records a vtable
// slot as the byte offset the disassembly indexes it by, not as a slot number.
inline void* VirtualFunction(void* object, uintptr_t slotOffset) {
  void* const* vtable = *reinterpret_cast<void* const**>(object);
  return vtable[slotOffset / sizeof(void*)];
}

// --- Edge functions, each resolved from g_base and typed exactly once ---------

inline void* NewView(size_t size) {
  using Fn = void*(__fastcall*)(size_t);
  return reinterpret_cast<Fn>(At(rva::CheckedOperatorNew3))(size);
}

inline void ConstructContainer(void* container, void* browserView) {
  using Fn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::EdgeContentsContainerView_ctor))(container,
                                                                browserView);
}

inline void AddChildViewAt(void* parent, void* child, size_t index) {
  using Fn = void*(__fastcall*)(void*, void*, size_t);
  reinterpret_cast<Fn>(At(rva::AddChildViewAtImpl))(parent, child, index);
}

// Appends a raw_ptr<ContentsContainerView> to the MCV container vector; `slot`
// points to an 8-byte stack temp holding the container pointer.
inline void ContainersEmplaceBack(void* vector, void* slot) {
  using Fn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::Containers_emplace_back))(vector, slot);
}

inline void SetViewVisible(void* view, bool isVisible) {
  using Fn = void(__fastcall*)(void*, bool);
  reinterpret_cast<Fn>(At(rva::View_SetVisible))(view, isVisible);
}

inline bool ViewGetVisible(void* view) {
  using Fn = bool(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::View_GetVisible))(view);
}

// Re-draw each pane's border/overlay from MCV_active_index (loops every container).
inline void McvUpdateBorders(void* mcv) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(
      At(rva::MultiContentsView_UpdateContentsBorderAndOverlay))(mcv);
}

inline void InvalidateLayout(void* view) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::View_InvalidateLayout))(view);
}
// Force a synchronous relayout now (InvalidateLayout only marks dirty; a pure
// tree edit with no data-model change needs this to re-run the layout detour).
inline void LayoutImmediately(void* view) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::View_LayoutImmediately))(view);
}

// std::vector<ChildLayout>::emplace_back<View*, bool, const gfx::Rect&>.
// `viewSlot` -> View*, `visibleSlot` -> bool, `rectSlot` -> gfx::Rect (our Rect).
inline void EmplaceChildLayout(void* vector, void* viewSlot, void* visibleSlot,
                               void* rectSlot) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::ChildLayouts_emplace_back))(
      vector, viewSlot, visibleSlot, rectSlot);
}

// Attach a WebContents to a ContentsWebView (the real render path).
inline void ContentsWebViewSetContents(void* contentsWebView,
                                       void* webContents) {
  using Fn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::ContentsWebView_SetWebContents))(contentsWebView,
                                                                webContents);
}

// Force a (background) WebContents to the visible state so the pane paints.
inline void WebContentsWasShown(void* webContents) {
  using Fn = void*(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::WebContents_WasShown))(webContents);
}

// --- native split data-model helpers ---------------------------------------
//
// A split id is a 16-byte base::Token; every `splitId` / `outSplitId` below
// points at one.

// Edge's non-View operator new (so a buffer we hand to a split primitive can be
// reallocated/freed by Edge's own allocator).
inline void* EdgeNew(size_t size) {
  using Fn = void*(__fastcall*)(size_t);
  return reinterpret_cast<Fn>(At(rva::EdgeOperatorNew))(size);
}
// Edge's operator delete, for memory Edge allocated and handed to us.
inline void EdgeDelete(void* memory) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::EdgeOperatorDelete))(memory);
}
// base::Token::CreateRandom(out base::Token*) — a valid split id.
inline void CreateRandomToken(void* outToken) {
  using Fn = void*(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::TokenCreateRandom))(outToken);
}

// The split primitives' index argument: {int* data; size_t size; size_t cap}
// (NOT libc++ vector) — confirmed from the caller storing rodata {1,1} at +8.
struct SplitIndexVec {
  int* data;
  size_t size;
  size_t capacity;
};

// Build a SplitIndexVec of `count` tab indices in an Edge-allocated buffer.
inline SplitIndexVec MakeIndexVec(const int* indices, size_t count) {
  int* buffer = static_cast<int*>(EdgeNew(count * sizeof(int)));
  for (size_t i = 0; i < count; i++) {
    buffer[i] = indices[i];
  }
  return SplitIndexVec{buffer, count, count};
}

// TabStripModel members + the split primitives (docs/re/split-grow-impl.md).
inline void* RootCollection(void* tsm) {
  return Field<void*>(tsm, off::TSM_collection);
}
inline void* ActiveTab(void* tsm) {
  return Field<void*>(tsm, off::TSM_active_tab);
}

// TabModel virtuals, called through the tab's vtable.
inline void* TabContents(void* tab) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(
      VirtualFunction(tab, off::TABMODEL_vtbl_GetContents))(tab);
}
inline bool TabIsSplit(void* tab) {
  using Fn = bool(__fastcall*)(void*);
  return reinterpret_cast<Fn>(VirtualFunction(tab, off::TABMODEL_vtbl_IsSplit))(
      tab);
}
// Fills `outOptionalSplitId`, a 24-byte optional<SplitTabId>: the Token @0, its
// has-value bool @0x10.
inline void TabGetSplit(void* tab, void* outOptionalSplitId) {
  using Fn = void(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(VirtualFunction(tab, off::TABMODEL_vtbl_GetSplit))(
      tab, outOptionalSplitId);
}

inline void* GetSplitData(void* tsm, void* splitId) {
  using Fn = void*(__fastcall*)(void*, void*);
  return reinterpret_cast<Fn>(At(rva::GetSplitData))(tsm, splitId);
}
// Fills `outVector`, a 24-byte {data, size, capacity} vector.
inline void GetTabsAndIndicesInSplit(void* tsm, void* outVector,
                                     void* splitId) {
  using Fn = void*(__fastcall*)(void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::GetTabsAndIndicesInSplit))(tsm, outVector,
                                                          splitId);
}
inline void* GetTabAtIndexRecursive(void* rootCollection, int index) {
  using Fn = void*(__fastcall*)(void*, int);
  return reinterpret_cast<Fn>(At(rva::GetTabAtIndexRecursive))(rootCollection,
                                                               index);
}
// GetIndexOfTabRecursive(rcx=collection, rdx=out{size_t index; bool found}, r8=tab).
// Returns -1 when the tab is not in the collection.
inline int GetIndexOfTabRecursive(void* rootCollection, void* tabModel) {
  struct {
    uint64_t index;
    uint8_t isFound;
    uint8_t padding[7];
  } result = {};
  using Fn = void(__fastcall*)(void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::GetIndexOfTabRecursive))(rootCollection, &result,
                                                        tabModel);
  return result.isFound ? static_cast<int>(result.index) : -1;
}
// Close the tab at strip `index` as a user gesture (Edge's own X does this).
inline void CloseTabAt(void* tsm, int index) {
  using CloseFn = void(__fastcall*)(void*, int, uint32_t);
  reinterpret_cast<CloseFn>(At(rva::TabStripModel_CloseWebContentsAt))(
      tsm, index, off::TSM_close_user_gesture);
}

// Activate the tab at strip `index` (no gesture metrics: details zeroed).
inline void ActivateTabAt(void* tsm, int index) {
  uint8_t gestureDetails[off::TSM_gesture_details_size] = {};
  using ActivateFn = void(__fastcall*)(void*, int, const void*);
  reinterpret_cast<ActivateFn>(At(rva::TabStripModel_ActivateTabAt))(
      tsm, index, gestureDetails);
}

// The tab model at strip `index`, or null.
inline void* TabModelAt(void* tsm, int index) {
  void* root = RootCollection(tsm);
  return root && index >= 0 ? GetTabAtIndexRecursive(root, index) : nullptr;
}

// The strip index of the tab showing `webContents`, or -1.
inline int IndexOfWebContents(void* tsm, void* webContents) {
  using IndexFn = int(__fastcall*)(void*, void*);
  return reinterpret_cast<IndexFn>(
      At(rva::TabStripModel_GetIndexOfWebContents))(tsm, webContents);
}

// The tab model showing `webContents` in `tsm`'s strip, or null (also for a
// null `tsm`).
inline void* TabShowing(void* tsm, void* webContents) {
  return tsm ? TabModelAt(tsm, IndexOfWebContents(tsm, webContents)) : nullptr;
}

// Move the tab at strip `index` so it ends up at `toIndex`, leaving the
// selection alone.
inline void MoveTabTo(void* tsm, int index, int toIndex) {
  using MoveFn = int(__fastcall*)(void*, int, int, bool);
  reinterpret_cast<MoveFn>(At(rva::TabStripModel_MoveWebContentsAt))(
      tsm, index, toIndex, false);
}

inline void RemoveSplitImpl(void* tsm, void* splitId, int flag) {
  using Fn = void*(__fastcall*)(void*, void*, int);
  reinterpret_cast<Fn>(At(rva::TabStripModel_RemoveSplitImpl))(tsm, splitId,
                                                               flag);
}
// 7 args: 4 register + 3 stack ([rsp+0x20/0x28/0x30]).
inline void AddToSplitImpl(void* tsm, void* outSplitId, void* splitId,
                           SplitIndexVec* indices, int anchor, void* visualData,
                           int flag) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*, int, void*, int);
  reinterpret_cast<Fn>(At(rva::TabStripModel_AddToSplitImpl))(
      tsm, outSplitId, splitId, indices, anchor, visualData, flag);
}
// AddToNewSplit(tsm, out base::Token*, SplitIndexVec*, SplitTabVisualData*) — creates
// a 2-pane split (its size==1 branch adds the active tab).
inline void AddToNewSplit(void* tsm, void* outSplitId, SplitIndexVec* indices,
                          void* visualData) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::TabStripModel_AddToNewSplit))(
      tsm, outSplitId, indices, visualData);
}
// UpdateTabInSplit(tsm, anchor=last split member TabModel*, int newTabIndex, int flag)
// — Edge's own "add a tab to the current split": runs the whole remove+move+add
// transaction internally (how OpenTabInCurrentSplitWindow adds a pane).
inline void UpdateTabInSplit(void* tsm, void* anchorMember, int newTabIndex,
                             int flag) {
  using Fn = void*(__fastcall*)(void*, void*, int, int);
  reinterpret_cast<Fn>(At(rva::TabStripModel_UpdateTabInSplit))(
      tsm, anchorMember, newTabIndex, flag);
}

// HWND -> aura::Window* -> BrowserView*, the bridge to reach a browser from a
// Win32 window handle (Chromium is Aura; GetBrowserViewForNativeWindow wants the
// aura window, not the HWND).
inline void* AuraWindowForHWND(void* hwnd) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::GetContentWindowForHWND))(hwnd);
}
inline void* BrowserViewForAura(void* auraWindow) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::BrowserView_ForNativeWindow))(auraWindow);
}

// TabStripModel::GetWebContentsAt — bounds-checked by the callee; caller must
// still pass 0 <= index < TabCount or it aborts.
inline void* TabWebContentsAt(void* tabStripModel, int index) {
  using Fn = void*(__fastcall*)(void*, int);
  return reinterpret_cast<Fn>(At(rva::TabStripModel_GetWebContentsAt))(
      tabStripModel, index);
}

// --- menu label plumbing (build a real libc++ std::u16string) ----------------
struct U8View {
  const char* data;
  size_t len;
};
struct U16String {  // opaque 24-byte libc++ std::u16string storage
  uint64_t _[3];
};
// base::UTF8ToUTF16(out /*rcx*/, string_view{ptr,len} /*rdx*/) -> fills `out`.
inline void Utf8ToUtf16(U16String* out, const U8View* view) {
  using Fn = void*(__fastcall*)(void*, const void*);
  reinterpret_cast<Fn>(At(rva::UTF8ToUTF16))(out, view);
}
// The same, from a NUL-terminated UTF-8 string.
inline void Utf8ToUtf16(U16String* out, const char* utf8) {
  const U8View view{utf8, strlen(utf8)};
  Utf8ToUtf16(out, &view);
}
// views::View::SetTooltipText(view, const std::u16string&), from UTF-8.
inline void ViewSetTooltip(void* view, const char* utf8) {
  U16String text{};
  Utf8ToUtf16(&text, utf8);
  using Fn = void(__fastcall*)(void*, const void*);
  reinterpret_cast<Fn>(At(rva::View_SetTooltipText))(view, &text);
}

inline int MouseClickCount(const void* mouseEvent) {
  using CountFn = int(__fastcall*)(const void*);
  return reinterpret_cast<CountFn>(At(rva::MouseEvent_GetClickCount))(
      mouseEvent);
}

inline bool ViewIsMouseHovered(void* view) {
  using HoveredFn = bool(__fastcall*)(void*);
  return reinterpret_cast<HoveredFn>(At(rva::View_IsMouseHovered))(view);
}

// Make a menu item measure itself again after `child` was added to it.
inline void MenuItemRemeasure(void* menuItem, void* child) {
  using ChangedFn = void(__fastcall*)(void*, void*);
  reinterpret_cast<ChangedFn>(At(rva::MenuItemView_ChildPreferredSizeChanged))(
      menuItem, child);
}

inline void* MenuItemById(void* rootMenuItem, int commandId) {
  using ByIdFn = void*(__fastcall*)(void*, int);
  return reinterpret_cast<ByIdFn>(At(rva::MenuItemView_GetMenuItemByID))(
      rootMenuItem, commandId);
}

// ui::SimpleMenuModel::AddItemWithIcon with `icon` (a gfx::VectorIcon) drawn
// the way Edge draws its split ⋯ items' icons.
inline void MenuAddItemWithIcon(void* model, int commandId,
                                const U16String* label, const void* icon) {
  alignas(16) uint8_t colorVariant[off::ColorVariant_size] = {};
  using ColorFn = void*(__fastcall*)(void*, int);
  reinterpret_cast<ColorFn>(At(rva::ColorVariant_ctor))(
      colorVariant, off::MENU_icon_color_id);
  uint64_t colorValue = 0;
  memcpy(&colorValue, colorVariant, sizeof(colorValue));
  alignas(16) uint8_t image[off::ImageModel_size_bound] = {};
  using FromIconFn =
      void*(__fastcall*)(void*, const void*, uint64_t, int, const void*);
  reinterpret_cast<FromIconFn>(At(rva::ImageModel_FromVectorIcon))(
      image, icon, colorValue, 0, nullptr);
  using AddFn = void(__fastcall*)(void*, int, const void*, const void*);
  reinterpret_cast<AddFn>(At(rva::SimpleMenuModel_AddItemWithIcon))(
      model, commandId, label, image);
  using DtorFn = void(__fastcall*)(void*);
  reinterpret_cast<DtorFn>(At(rva::ImageModel_dtor))(image);
}

// ui::SimpleMenuModel::AddSeparator(model, NORMAL_SEPARATOR).
inline void MenuAddSeparator(void* model) {
  using SeparatorFn = void(__fastcall*)(void*, int);
  reinterpret_cast<SeparatorFn>(At(rva::SimpleMenuModel_AddSeparator))(
      model, off::MENU_normal_separator);
}

// ui::SimpleMenuModel::AddItem(model /*rcx*/, int command_id /*edx*/, &u16string /*r8*/).
inline void MenuAddItem(void* model, int commandId, const U16String* label) {
  using Fn = void*(__fastcall*)(void*, int, const void*);
  reinterpret_cast<Fn>(At(rva::SimpleMenuModel_AddItem))(model, commandId,
                                                         label);
}

// A ui::SimpleMenuModel's items, by index (each call CHECKs the index).
inline size_t MenuItemCount(void* model) {
  using CountFn = size_t(__fastcall*)(void*);
  return reinterpret_cast<CountFn>(At(rva::SimpleMenuModel_GetItemCount))(
      model);
}
inline int MenuCommandIdAt(void* model, size_t index) {
  using IdFn = int(__fastcall*)(void*, size_t);
  return reinterpret_cast<IdFn>(At(rva::SimpleMenuModel_GetCommandIdAt))(model,
                                                                         index);
}
inline void MenuRemoveItemAt(void* model, size_t index) {
  using RemoveFn = void(__fastcall*)(void*, size_t);
  reinterpret_cast<RemoveFn>(At(rva::SimpleMenuModel_RemoveItemAt))(model,
                                                                    index);
}
// The label is copied into the item.
inline void MenuSetLabel(void* model, size_t index, const U16String* label) {
  using LabelFn = void(__fastcall*)(void*, size_t, const void*);
  reinterpret_cast<LabelFn>(At(rva::SimpleMenuModel_SetLabel))(model, index,
                                                               label);
}

// --- MultiContentsView field accessors (named, typed, one place each) ---------

// The container vector OBJECT begins at this address (pass to ContainersEmplaceBack).
inline void* McvContainerVector(void* mcv) {
  return reinterpret_cast<uint8_t*>(mcv) + off::MCV_containers_data;
}
// The vector's begin pointer: an array of ContentsContainerView*.
inline void** McvContainers(void* mcv) {
  return Field<void**>(mcv, off::MCV_containers_data);
}
inline void* McvContainerAt(void* mcv, int index) {
  return McvContainers(mcv)[index];
}
inline size_t McvCount(void* mcv) {
  return Field<size_t>(mcv, off::MCV_containers_count);
}
inline void* McvBrowserView(void* mcv) {
  return Field<void*>(mcv, off::MCV_browser_view);
}
inline uint32_t McvOrientation(void* mcv) {
  return Field<uint32_t>(mcv, off::MCV_orientation);
}
inline size_t ViewChildCount(void* view) {
  return Field<size_t>(view, off::VIEW_child_count);
}
// Child `index` (< ViewChildCount) of `view`.
inline void* ViewChildAt(void* view, size_t index) {
  return Field<void**>(view, off::VIEW_children)[index];
}
// Where `object`'s vtable sits in msedge.dll, to name its class from the PDB.
inline uintptr_t VtableRva(void* object) {
  return reinterpret_cast<uintptr_t>(Field<void*>(object, 0)) - g_base;
}

// CalculateProposedLayout runs on the LayoutManagerBase subobject embedded at
// MCV_layout_base.
inline void* McvFromLayoutBase(void* layoutBase) {
  return reinterpret_cast<uint8_t*>(layoutBase) - off::MCV_layout_base;
}
inline void* BrowserViewMcv(void* browserView) {
  return Field<void*>(browserView, off::BV_multi_contents_view);
}

// A view's current bounds in its parent's coordinates (views::View::bounds_).
inline Rect ViewBounds(void* view) {
  return Field<Rect>(view, off::VIEW_bounds);
}

// --- views helpers -------------------------------------------------------------

inline void* ViewParent(void* view) {
  return Field<void*>(view, off::VIEW_parent);
}
// The widget's root view: its coordinates are the window's client area in DIPs.
inline void* ViewRoot(void* view) {
  while (void* parent = ViewParent(view)) {
    view = parent;
  }
  return view;
}
// Convert `point` in place from `source`'s coordinates to `target`'s.
inline void ConvertPoint(void* source, void* target, Point* point) {
  using Fn = void(__fastcall*)(void*, void*, Point*);
  reinterpret_cast<Fn>(At(rva::View_ConvertPointToTarget))(source, target,
                                                           point);
}
// A view's bounds in screen DIPs.
inline Rect ViewBoundsInScreen(void* view) {
  Rect bounds{};
  using BoundsFn = void*(__fastcall*)(void*, Rect*);
  reinterpret_cast<BoundsFn>(At(rva::View_GetBoundsInScreen))(view, &bounds);
  return bounds;
}
// Convert `point` in place from screen DIPs to `view`'s coordinates.
inline void ConvertPointFromScreen(void* view, Point* point) {
  using ConvertFn = void(__fastcall*)(void*, Point*);
  reinterpret_cast<ConvertFn>(At(rva::View_ConvertPointFromScreen))(view,
                                                                    point);
}

// --- a page's navigation --------------------------------------------------------

// Whether `webContents` was opened by another page that can still script it.
inline bool HasOpener(void* webContents) {
  using OpenerFn = bool(__fastcall*)(void*);
  return reinterpret_cast<OpenerFn>(At(rva::WebContentsImpl_HasOpener))(
      webContents);
}

// Load `url` (a GURL*) in `webContents`, as a link would.
inline void NavigateTo(void* webContents, const void* url) {
  alignas(16) uint8_t params[off::LoadURLParams_size] = {};
  using ParamsFn = void*(__fastcall*)(void*, const void*);
  reinterpret_cast<ParamsFn>(At(rva::LoadURLParams_ctor))(params, url);
  using ControllerFn = const void*(__fastcall*)(void*);
  const void* controller = reinterpret_cast<ControllerFn>(
      At(rva::WebContentsImpl_GetController))(webContents);
  alignas(8) uint8_t navigationHandle[off::WeakPtr_size] = {};
  using LoadFn = void(__fastcall*)(const void*, void*, const void*);
  reinterpret_cast<LoadFn>(At(rva::NavigationControllerImpl_LoadURLWithParams))(
      controller, navigationHandle, params);
  using DtorFn = void(__fastcall*)(void*);
  reinterpret_cast<DtorFn>(At(rva::WeakReference_dtor))(navigationHandle);
  reinterpret_cast<DtorFn>(At(rva::LoadURLParams_dtor))(params);
}

// Open the GURL `gurl` in a new background tab at strip `index`
// (chrome::AddTabAt's worker); returns its WebContents, or null. `browser` is
// what BrowserOf returns: the BrowserWindowInterface view of Edge's Browser
// (tab_strip_model at +0x30), as AddAndReturnTabAt takes it.
inline void* AddBackgroundTabForGurl(void* browser, const void* gurl,
                                     int index) {
  alignas(8) uint8_t noGroup[off::OptionalTabGroupId_size] = {};
  constexpr uint32_t kNoExtraTransition = 0;
  constexpr bool kIsForeground = false;
  constexpr bool kIsPinned = false;
  constexpr uint64_t kNoOptionalInt = 0;  // an empty optional<int>
  using AddFn = void*(__fastcall*)(void*, const void*, uint32_t, int, bool,
                                   const void*, bool, uint64_t);
  return reinterpret_cast<AddFn>(At(rva::Chrome_AddAndReturnTabAt))(
      browser, gurl, kNoExtraTransition, index, kIsForeground, noGroup,
      kIsPinned, kNoOptionalInt);
}
// The same, from a UTF-8 URL string.
inline void* AddBackgroundTab(void* browser, const char* url, int index) {
  const struct {
    const char* data;
    size_t size;
  } urlView = {url, std::strlen(url)};
  alignas(16) uint8_t gurl[off::GURL_size] = {};
  using GurlFn = void*(__fastcall*)(void*, const void*);
  reinterpret_cast<GurlFn>(At(rva::GURL_ctor_string))(gurl, &urlView);
  void* webContents = AddBackgroundTabForGurl(browser, gurl, index);
  using DtorFn = void(__fastcall*)(void*);
  reinterpret_cast<DtorFn>(At(rva::GURL_dtor))(gurl);
  return webContents;
}

// The URL of the page `webContents` last committed (a const GURL&): the page
// Edge's split "Open this webpage in a new tab" copies.
inline const void* LastCommittedUrl(void* webContents) {
  using UrlFn = const void*(__fastcall*)(void*);
  return reinterpret_cast<UrlFn>(At(rva::WebContentsImpl_GetLastCommittedURL))(
      webContents);
}

// The page Edge's split button opens in its new pane: it offers the window's
// other tabs.
inline const char* StartingPageUrl() {
  return static_cast<const char*>(At(rva::EdgeUISplitWindowStartingPageURL));
}

// Whether `webContents` is Edge's split starting page, judged as Edge's split
// controller judges it: by the user data the page attaches to its contents.
inline bool IsStartingPage(void* webContents) {
  constexpr bool kIsJudgedByUrl = false;
  using StartingFn = bool(__fastcall*)(void*, bool);
  return reinterpret_cast<StartingFn>(At(
      rva::SplitWindowController_IsStartingPage))(webContents, kIsJudgedByUrl);
}

// A task's bound base::WeakPtr (its BindState's first bound argument): its
// pointer while the WeakPtr is valid, else null.
inline void* BoundWeakTarget(void* bindState) {
  void* flag = Field<void*>(bindState, off::BINDSTATE_first_bound_argument);
  const bool isValid =
      flag && Field<uint8_t>(flag, off::WEAKFLAG_invalidated) == 0;
  return isValid ? Field<void*>(bindState, off::BINDSTATE_first_bound_argument +
                                               sizeof(void*))
                 : nullptr;
}
// Null the bound WeakPtr's pointer, so the task finds nothing to act on and
// only releases its reference.
inline void ClearBoundWeakTarget(void* bindState) {
  Field<void*>(bindState, off::BINDSTATE_first_bound_argument + sizeof(void*)) =
      nullptr;
}

// The WebContents of the starting page a SplitWindowStartingUIHandler serves.
inline void* StartingPageOf(void* handler) {
  void* webUi = Field<void*>(handler, off::STARTINGHANDLER_web_ui);
  return webUi ? Field<void*>(webUi, off::WEBUI_web_contents) : nullptr;
}

// --- events -------------------------------------------------------------------

inline int EventType(void* event) {
  return Field<int32_t>(event, off::EVENT_type);
}
// A key event's ui::KeyboardCode (the Windows virtual-key code).
inline int KeyEventCode(void* keyEvent) {
  return Field<uint8_t>(keyEvent, off::KEYEVENT_key_code);
}

// --- a tab drag (TabDragController) ---------------------------------------------

inline bool IsDraggingTabs(void* controller) {
  return Field<int32_t>(controller, off::TABDRAG_state) ==
         off::TABDRAG_state_dragging_tabs;
}
inline size_t DraggedTabCount(void* controller) {
  return Field<size_t>(controller, off::TABDRAG_drag_data_count);
}
// The WebContents of dragged tab `i` (< DraggedTabCount).
inline void* DraggedContents(void* controller, size_t i) {
  auto* entries = Field<uint8_t*>(controller, off::TABDRAG_drag_data);
  return Field<void*>(entries + i * off::TABDRAGDATA_size,
                      off::TABDRAGDATA_contents);
}
// The tab strip the drag is attached to (a TabDragContext view).
inline void* DragContext(void* controller) {
  return Field<void*>(controller, off::TABDRAG_attached_context);
}
inline void* DragContextTabStripModel(void* context) {
  using ModelFn = void*(__fastcall*)(void*);
  return reinterpret_cast<ModelFn>(VirtualFunction(
      context, off::DRAGCONTEXT_vtbl_GetTabStripModel))(context);
}
// The tab view showing `webContents` in the drag's strip, or null.
inline void* DragContextTabFor(void* context, void* webContents) {
  using TabFn = void*(__fastcall*)(void*, void*);
  return reinterpret_cast<TabFn>(VirtualFunction(
      context, off::DRAGCONTEXT_vtbl_GetTabForContents))(context, webContents);
}

// --- the tab list's layout (TabStripLayoutHelper) ------------------------------

inline size_t TabLayoutSlotCount(void* helper) {
  return Field<size_t>(helper, off::TABLAYOUT_slot_count);
}
// The tab view in slot `index`, or null for a slot that holds no tab.
inline void* TabLayoutSlotTab(void* helper, size_t index) {
  auto* slots = Field<uint8_t*>(helper, off::TABLAYOUT_slots);
  uint8_t* slot = slots + index * off::TABSLOT_size;
  const bool isTab =
      Field<uint32_t>(slot, off::TABSLOT_type) == off::TABSLOT_type_tab;
  return isTab ? Field<void*>(slot, off::TABSLOT_view) : nullptr;
}
inline bool IsTabViewInSplit(void* tabView) {
  return (Field<uint8_t>(tabView, off::TABVIEW_split_has_value) & 1) != 0;
}
// Whether `other` is a tab of the same class as `tabView`, in the same split.
inline bool IsTabViewInSameSplit(void* tabView, void* other) {
  const bool isSameClass = Field<void*>(other, 0) == Field<void*>(tabView, 0);
  return isSameClass && IsTabViewInSplit(other) &&
         memcmp(reinterpret_cast<uint8_t*>(tabView) + off::TABVIEW_split_token,
                reinterpret_cast<uint8_t*>(other) + off::TABVIEW_split_token,
                off::TABVIEW_split_token_size) == 0;
}
// CalculateIdealBounds's result: one rect per slot.
inline Rect* IdealBoundsRects(void* result) {
  return Field<Rect*>(result, off::IDEALBOUNDS_data);
}
inline size_t IdealBoundsCount(void* result) {
  return Field<size_t>(result, off::IDEALBOUNDS_count);
}

// Where a mouse event happened, in the window's DIPs (its root location).
inline Point EventRootLocation(const void* event) {
  const auto* location = reinterpret_cast<const float*>(
      reinterpret_cast<const uint8_t*>(event) + off::EVENT_root_location);
  return Point{static_cast<int32_t>(location[0]),
               static_cast<int32_t>(location[1])};
}
inline void ViewReorderChild(void* parent, void* child, size_t index) {
  using Fn = void(__fastcall*)(void*, void*, size_t);
  reinterpret_cast<Fn>(At(rva::View_ReorderChildView))(parent, child, index);
}
inline void ViewRemoveChild(void* parent, void* child) {
  using Fn = void(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::View_RemoveChildView))(parent, child);
}

// --- per-pane hover controls -------------------------------------------------

inline void* ContainerControlButtons(void* container) {
  return Field<void*>(container, off::ECCV_control_buttons);
}

// EdgeContentsContainerView::SetControlButtons(unique_ptr<ControlButtons>*), a
// virtual on its control-buttons interface subobject. The callee takes the bar
// out of `*ownedBar` and nulls it.
inline void ContainerSetControlButtons(void* container, void** ownedBar) {
  void* buttonsInterface =
      reinterpret_cast<uint8_t*>(container) + off::ECCV_buttons_iface;
  using Fn = void(__fastcall*)(void*, void**);
  reinterpret_cast<Fn>(
      VirtualFunction(buttonsInterface, off::ECCV_vtbl_SetControlButtons))(
      buttonsInterface, ownedBar);
}

// Build a ControlButtons bar and hand it to `container`, exactly as
// EnsureControlButtons does for pane 1 (`not_first` = true there).
inline void GiveControlButtons(void* container, void* browserView,
                               bool not_first) {
  void* bar = NewView(off::ControlButtons_size);
  using Ctor = void*(__fastcall*)(void*, void*, bool);
  reinterpret_cast<Ctor>(At(rva::ControlButtons_ctor))(bar, browserView,
                                                       not_first);
  void* ownedBar = bar;  // stands in for the unique_ptr the callee empties
  ContainerSetControlButtons(container, &ownedBar);
}

// Build an EdgeMultiContentsResizeArea for `mcv` in `view` (EdgeResizeArea_size
// bytes from NewView); it starts hidden.
inline void ConstructResizeArea(void* view, void* mcv) {
  using CtorFn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<CtorFn>(At(rva::EdgeMultiContentsResizeArea_ctor))(view,
                                                                      mcv);
}

// Edge's own (single, two-pane) resize handle of a split.
inline void* McvResizeArea(void* mcv) {
  return Field<void*>(mcv, off::MCV_resize_area);
}

// The X button of a pane's hover bar.
inline void* ControlButtonsCloseButton(void* bar) {
  return Field<void*>(bar, off::ControlButtons_close_button);
}

// A plain views::View (no layout manager: its children stay where they are
// put), not yet parented.
inline void* NewPlainView() {
  void* view = NewView(off::View_size);
  using CtorFn = void*(__fastcall*)(void*);
  reinterpret_cast<CtorFn>(At(rva::View_ctor))(view);
  return view;
}

inline void SetViewBounds(void* view, const Rect& bounds) {
  using SetBoundsFn = void(__fastcall*)(void*, const Rect*);
  reinterpret_cast<SetBoundsFn>(At(rva::View_SetBoundsRect))(view, &bounds);
}

inline Size ViewPreferredSize(void* view) {
  Size size{};
  const uint8_t unboundedSizeBounds[off::SizeBounds_size] = {};
  using PreferredFn = void*(__fastcall*)(void*, Size*, const void*);
  reinterpret_cast<PreferredFn>(At(rva::View_GetPreferredSize))(
      view, &size, unboundedSizeBounds);
  return size;
}

inline void* MenuItemIconView(void* menuItem) {
  return Field<void*>(menuItem, off::MENUITEM_icon_view);
}

// Close the open menu, if any, as MenuRunner::Cancel does.
inline void CancelActiveMenu() {
  using ActiveFn = const void*(__fastcall*)();
  const void* controller =
      reinterpret_cast<ActiveFn>(At(rva::MenuController_GetActiveInstance))();
  if (!controller) {
    return;
  }
  using CancelFn = void(__fastcall*)(const void*, int, int);
  reinterpret_cast<CancelFn>(At(rva::MenuController_Cancel))(
      controller, off::MENU_exit_all, 0);
}

// The MenuItemView whose views::LayoutDelegate base `layoutDelegate` is.
inline void* MenuItemOfLayoutDelegate(void* layoutDelegate) {
  return static_cast<uint8_t*>(layoutDelegate) - off::MENUITEM_layout_delegate;
}

inline int ProposedHostHeight(void* proposedLayout) {
  return Field<Size>(proposedLayout, off::PL_host_size).height;
}

// The bounds a views::ProposedLayout gives `child`, or null if it has none.
inline Rect* ProposedChildBounds(void* proposedLayout, const void* child) {
  auto* entries = Field<uint8_t*>(proposedLayout, off::PL_child_layouts);
  const auto count = Field<size_t>(proposedLayout, off::PL_child_count);
  for (size_t i = 0; i < count; i++) {
    uint8_t* entry = entries + i * off::CL_stride;
    const bool isChildEntry = Field<void*>(entry, off::CL_child) == child;
    if (isChildEntry) {
      return reinterpret_cast<Rect*>(entry + off::CL_bounds);
    }
  }
  return nullptr;
}

inline void SetViewPreferredSize(void* view, const Size& size) {
  struct OptionalSize {  // std::optional<gfx::Size>
    Size size;
    bool hasValue;
  };
  const OptionalSize preferred{size, true};
  using PreferredFn = void(__fastcall*)(void*, const OptionalSize*);
  reinterpret_cast<PreferredFn>(At(rva::View_SetPreferredSize))(view,
                                                                &preferred);
}

// One round icon button in the hover bar's style, not yet parented.
inline void* NewControlButton(const void* icon) {
  void* button = NewView(off::ControlButtons_button_size);
  using CtorFn = void*(__fastcall*)(void*, const void*);
  reinterpret_cast<CtorFn>(At(rva::ControlButtons_Button_ctor))(button, icon);
  return button;
}

// Add a button to a pane's hover bar, exactly as Edge adds ⋯ and X. `icon` is a
// gfx::VectorIcon*; `clickable` = false wires no click handler.
inline void* ControlButtonsAddButton(void* bar, int stringId, const void* icon,
                                     bool clickable) {
  using Fn = void*(__fastcall*)(void*, int, const void*, bool);
  return reinterpret_cast<Fn>(At(rva::ControlButtons_AddButton))(
      bar, stringId, icon, clickable);
}
// Re-run the bar's own layout so its width follows its visible buttons.
inline void ControlButtonsRelayout(void* bar) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::ControlButtons_InitLayout))(bar);
}

// Watch a pane's WebContents for mouse input so hovering it shows its bar.
inline void ListenSplitInput(void* webContents) {
  using Fn = void(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::SplitWindow_ListenWebContentsInputEvent))(
      webContents, nullptr);
}

// The browser's SplitWindowController (null if it has none).
inline void* SplitWindowControllerFor(void* browser) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::SplitWindowController_FromBrowser))(
      browser);
}
// Tell the open starting pages of `controller`'s window that `webContents` is
// no longer a tab to offer: each drops its card
// (StartingPageNotifierImpl::OnTabClosing). No notifier yet means no starting
// page has opened, so none to tell.
inline void WithdrawFromStartingPages(void* controller, void* webContents) {
  const void* notifier =
      controller ? Field<void*>(controller,
                                off::SPLITCONTROLLER_starting_page_notifier)
                 : nullptr;
  if (!notifier) {
    return;
  }
  using ClosingFn = void(__fastcall*)(const void*, void*);
  reinterpret_cast<ClosingFn>(At(rva::StartingPageNotifierImpl_OnTabClosing))(
      notifier, webContents);
}
// What a pane's hover runs: show the bar of the container showing
// `webContents`, hide the rest.
inline bool SplitWindowControllerShowControlButton(void* controller,
                                                   void* webContents) {
  using Fn = bool(__fastcall*)(void*, void*);
  return reinterpret_cast<Fn>(At(rva::SplitWindowController_ShowControlButton))(
      controller, webContents);
}

// Run the hover's effect for the pane showing `webContents` directly (the test
// channel's stand-in for a real mouse-enter, which needs the real cursor).
inline bool ShowControlButtonFor(void* browser, void* webContents) {
  void* controller = SplitWindowControllerFor(browser);
  if (!controller) {
    return false;
  }
  return SplitWindowControllerShowControlButton(controller, webContents);
}

// A container's ContentsWebView (the pane's web view).
inline void* ContainerWebView(void* container) {
  return Field<void*>(container, off::CCV_contents_web_view);
}
// The WebContents a container's web view is showing (null if none).
inline void* ContainerWebContents(void* container) {
  return Field<void*>(ContainerWebView(container), off::CWV_web_contents);
}

// --- Browser / TabStripModel field accessors ---------------------------------
inline void* BrowserOf(void* browserView) {
  return Field<void*>(browserView, off::BV_browser);
}
inline void* TabStripModelOf(void* browser) {
  return Field<void*>(browser, off::BROWSER_tab_strip_model);
}
// The TabStripModel behind an MCV's browser view (null if none).
inline void* McvTabStripModel(void* mcv) {
  void* browserView = mcv ? McvBrowserView(mcv) : nullptr;
  return browserView ? TabStripModelOf(BrowserOf(browserView)) : nullptr;
}
inline int TabCount(void* tabStripModel) {
  void* collection = Field<void*>(tabStripModel, off::TSM_collection);
  return collection ? Field<int32_t>(collection, off::TABCOLL_count) : 0;
}

// A split's members as Edge lists them (GetTabsAndIndicesInSplit): a vector
// {data, size, capacity} of {TabModel*, int} entries that Edge allocates for
// the caller, freed with Edge's allocator when this goes out of scope.
struct SplitMembers {
  SplitMembers(void* tsm, void* splitId) {
    GetTabsAndIndicesInSplit(tsm, &list, splitId);
  }
  ~SplitMembers() {
    if (list.data) {
      EdgeDelete(list.data);
    }
  }
  SplitMembers(const SplitMembers&) = delete;
  SplitMembers& operator=(const SplitMembers&) = delete;

  int Count() const {
    return list.data ? static_cast<int>(list.size) : 0;
  }
  // Member `i`'s tab model (0 <= i < Count()).
  void* TabAt(int i) const {
    return Field<void*>(
        list.data + static_cast<size_t>(i) * off::SPLITMEMBER_size, 0);
  }

  struct {
    uint8_t* data = nullptr;
    uint64_t size = 0;
    uint64_t capacity = 0;
  } list;
};

// How many panes this MCV should show: the active tab's split member count
// (>=2), or 1 when the active tab isn't in a split (collapsed / normal). The
// data model is the single source of truth; the container vector is a cache that
// only grows, so surplus containers are hidden rather than destroyed.
inline int McvActiveSplitPanes(void* mcv) {
  void* tsm = McvTabStripModel(mcv);
  if (!tsm) {
    return 1;
  }
  void* activeTab = ActiveTab(tsm);
  const bool isActiveTabInSplit = activeTab && TabIsSplit(activeTab);
  if (!isActiveTabInSplit) {
    return 1;
  }
  struct {
    uint8_t token[16];
    uint8_t hasValue;
    uint8_t padding[7];
  } optionalSplitId = {};
  TabGetSplit(activeTab, &optionalSplitId);
  if (!optionalSplitId.hasValue) {
    return 1;
  }
  const int memberCount = SplitMembers(tsm, optionalSplitId.token).Count();
  return memberCount < 1 ? 1 : memberCount;
}

// TabStripModel from a top-level Win32 browser window handle (no split needed).
inline void* TsmForWindow(void* hwnd) {
  void* auraWindow = AuraWindowForHWND(hwnd);
  if (!auraWindow) {
    return nullptr;
  }
  void* browserView = BrowserViewForAura(auraWindow);
  return browserView ? TabStripModelOf(BrowserOf(browserView)) : nullptr;
}

// Run Edge's own split-view update for `browserView` as if `webContents` just
// changed: when that tab is no longer in a split, Edge collapses the view to
// one full-window pane (CloseSplitView). Used after a split is dissolved.
inline void MaybeUpdateSplitView(void* browserView, void* webContents) {
  using Fn = void(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::BrowserView_MaybeUpdateSplitView))(browserView,
                                                                  webContents);
}

// MultiContentsView from a top-level Win32 browser window handle.
inline void* McvForWindow(void* hwnd) {
  void* auraWindow = AuraWindowForHWND(hwnd);
  if (!auraWindow) {
    return nullptr;
  }
  void* browserView = BrowserViewForAura(auraWindow);
  return browserView ? BrowserViewMcv(browserView) : nullptr;
}

// --- theming -----------------------------------------------------------------
// content::BrowserContext* (the Profile) behind a WebContents.
inline void* BrowserContextOf(void* webContents) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::WebContentsImpl_GetBrowserContext))(
      webContents);
}

// The Profile (BrowserContext) behind a TabStripModel's active tab, our bridge
// to that profile's ThemeService and ExtensionRegistry.
inline void* ProfileForTabStrip(void* tsm) {
  void* tab = tsm ? ActiveTab(tsm) : nullptr;
  void* webContents = tab ? TabContents(tab) : nullptr;
  return webContents ? BrowserContextOf(webContents) : nullptr;
}

// The per-profile ThemeService (one serves the whole profile).
inline void* ThemeServiceForProfile(void* profile) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::ThemeServiceFactory_GetForProfile))(
      profile);
}

// A libc++ std::string holding a theme id, to pass by const-ref. One reused
// DLL-lifetime buffer: we clear it in place (keeps any heap buffer) and append
// the id, so we never pair an operator delete ourselves and the one small
// buffer is reclaimed when Edge exits. Not re-entrant, which the UI-thread-only
// theme callers satisfy.
inline const void* ThemeIdString(const char* themeId) {
  static uint64_t storage[3] = {0, 0, 0};  // an empty libc++ std::string
  // Clear in place (size 0, keeps any heap buffer): assign(size_type n=0, char).
  using ClearFn = void(__fastcall*)(void*, size_t, char);
  reinterpret_cast<ClearFn>(At(rva::BasicStringChar_assignFill))(&storage, 0,
                                                                 0);
  // Append the NUL-terminated id (strlen inside), building it fresh each call.
  using AppendFn = void(__fastcall*)(void*, const char*);
  reinterpret_cast<AppendFn>(At(rva::BasicStringChar_append))(&storage,
                                                              themeId);
  return &storage;
}

// Where an installed extension sits in the profile's registry.
enum class ExtensionState : uint8_t {
  kAbsent,    // not installed
  kEnabled,   // loaded and on (the active theme is)
  kDisabled,  // installed but off (an inactive theme, or one not yet accepted)
};

// The installed extension with id `themeId` in `profile` and which set holds it
// (ExtensionSet::GetByID on the registry's disabled, then enabled, set).
inline void* ExtensionById(void* profile, const char* themeId,
                           ExtensionState* state) {
  *state = ExtensionState::kAbsent;
  using RegistryFn = void*(__fastcall*)(void*);
  void* registry =
      reinterpret_cast<RegistryFn>(At(rva::ExtensionRegistry_Get))(profile);
  if (!registry) {
    return nullptr;
  }
  using GetByIdFn = void*(__fastcall*)(void*, const void*);
  const auto getById =
      reinterpret_cast<GetByIdFn>(At(rva::ExtensionSet_GetByID));
  void* extension =
      getById(reinterpret_cast<uint8_t*>(registry) + off::REGISTRY_disabled_set,
              ThemeIdString(themeId));
  if (extension) {
    *state = ExtensionState::kDisabled;
    return extension;
  }
  extension =
      getById(reinterpret_cast<uint8_t*>(registry) + off::REGISTRY_enabled_set,
              ThemeIdString(themeId));
  if (extension) {
    *state = ExtensionState::kEnabled;
  }
  return extension;
}

// Turn on the installed-but-disabled extension `themeId` (the call Edge's own
// Enable button makes). It clears whatever disabled it, an external extension
// the user never accepted or a theme that was switched away from. Only call for
// an extension ExtensionById reported kDisabled: EnableExtension CHECKs that it
// is in the disabled set.
inline void EnableDisabledExtension(void* profile, const char* themeId) {
  using GetRegistrarFn = void*(__fastcall*)(void*);
  const void* registrar = reinterpret_cast<GetRegistrarFn>(
      At(rva::ExtensionRegistrar_Get))(profile);
  if (!registrar) {
    return;
  }
  using EnableFn = void(__fastcall*)(const void*, const void*);
  reinterpret_cast<EnableFn>(At(rva::ExtensionRegistrar_EnableExtension))(
      registrar, ThemeIdString(themeId));
}

// Make `extension` the profile's active theme, live: DoSetTheme builds the theme
// pack and (suppress=false) notifies, repainting the browser in place. Unlike
// UseDefaultTheme, DoSetTheme takes the primary `this` (its large member offsets
// prove it; the +0x10 secondary base faults here).
inline void ThemeSetFromExtension(void* themeService, void* extension) {
  using Fn = void(__fastcall*)(void*, void*, bool);
  reinterpret_cast<Fn>(At(rva::ThemeService_DoSetTheme))(themeService,
                                                         extension, false);
}

// Drop any extension theme and return the profile to the default theme, live.
// UseDefaultTheme is an override in a base at ThemeService + 0x10 (it adjusts
// this - 0x10 back to the primary), so it takes that secondary-base pointer.
inline void ThemeUseDefault(void* themeService) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::ThemeService_UseDefaultTheme))(
      reinterpret_cast<uint8_t*>(themeService) + 0x10);
}

// --- web protocol handlers -------------------------------------------------------
// The handler bubble model's OnDoneButtonClicked vtable slot, as an RVA.
inline constexpr uintptr_t kRphDoneSlotRva =
    rva::ContentSettingRPHBubbleModel_vftable +
    off::BubbleModel_OnDoneButtonClicked_slot;

// The function the handler bubble's Done slot points at now.
inline void* RphDoneSlot() {
  return *reinterpret_cast<void**>(At(kRphDoneSlotRva));
}

// Whether the Done slot still holds Edge's own empty virtual (the build check
// for the slot, before we ever write it).
inline bool RphDoneSlotIsStock() {
  return RphDoneSlot() == At(rva::ContentSettingBubbleModel_EmptyVirtual);
}

// Point the handler bubble's Done slot at `function` (the vtable lives in
// read-only data, so the page is made writable for the one pointer).
inline void SetRphDoneSlot(void* function) {
  void* address = At(kRphDoneSlotRva);
  DWORD oldProtection = 0;
  if (VirtualProtect(address, sizeof(void*), PAGE_READWRITE, &oldProtection)) {
    *reinterpret_cast<void**>(address) = function;
    VirtualProtect(address, sizeof(void*), oldProtection, &oldProtection);
  }
}

// Put Edge's own empty virtual back in the Done slot.
inline void RestoreRphDoneSlot() {
  SetRphDoneSlot(At(rva::ContentSettingBubbleModel_EmptyVirtual));
}

// A permission prompt's decision, as PermissionDecided reads it.
struct PermissionDecision {
  bool hasValue;  // false when the prompt was dismissed without a choice
  int value;      // 0 = Allow; anything else Edge treats as Block
};

inline PermissionDecision ReadPermissionDecision(const void* decision) {
  void* raw = const_cast<void*>(decision);
  return {Field<uint8_t>(raw, off::PERMISSION_DECISION_has_value) == 1,
          Field<int32_t>(raw, off::PERMISSION_DECISION_value)};
}

// Room for one custom_handlers::ProtocolHandler that our DLL owns: the bytes
// are ours, the strings and GURL inside are built and freed by Edge's own copy
// constructor and destructor.
struct alignas(16) ProtocolHandlerStorage {
  uint8_t bytes[off::PROTOCOL_HANDLER_size];
};

// The profile's ProtocolHandlerRegistry (created if it does not exist yet).
inline void* ProtocolHandlerRegistryFor(void* profile) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(
      At(rva::ProtocolHandlerRegistryFactory_GetForBrowserContext))(profile);
}

// How many handlers the registry's live ignored list holds.
inline size_t IgnoredHandlerCount(void* registry) {
  return Field<size_t>(registry, off::RPH_REGISTRY_ignored_size);
}

// Copy-construct the registry's ignored handler `index` into `storage`, so it
// stays valid while the list it came from is erased from. Pair every copy with
// DestroyProtocolHandler.
inline void CopyIgnoredHandler(void* registry, size_t index,
                               ProtocolHandlerStorage* storage) {
  auto* data = Field<uint8_t*>(registry, off::RPH_REGISTRY_ignored_data);
  using Fn = void*(__fastcall*)(void*, const void*);
  reinterpret_cast<Fn>(At(rva::ProtocolHandler_CopyConstructor))(
      storage, data + index * off::PROTOCOL_HANDLER_size);
}

inline void DestroyProtocolHandler(ProtocolHandlerStorage* storage) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::ProtocolHandler_Destructor))(storage);
}

// Lift the block on `handler` (a copy, never an element of the ignored list)
// and save the profile's prefs, as the bubble's Allow does before it accepts.
inline void RemoveIgnoredHandler(void* registry,
                                 const ProtocolHandlerStorage* handler) {
  constexpr bool kShouldSave = true;
  using Fn = void(__fastcall*)(void*, const void*, bool);
  reinterpret_cast<Fn>(At(rva::ProtocolHandlerRegistry_RemoveIgnoredHandler))(
      registry, handler, kShouldSave);
}

// --- the browser assistant (docs/re/gemini-in-chrome.md) ---------------------

// A read-only view of a libc++ std::u16string's characters.
struct U16View {
  const char16_t* data;
  size_t length;
};
inline U16View U16StringView(const void* string) {
  auto* bytes = static_cast<const uint8_t*>(string);
  const bool isLong =
      (bytes[off::U16STRING_short_size] & off::U16STRING_long_flag) != 0;
  if (isLong) {
    return U16View{
        *reinterpret_cast<const char16_t* const*>(bytes),
        *reinterpret_cast<const size_t*>(bytes + off::U16STRING_long_size)};
  }
  return U16View{reinterpret_cast<const char16_t*>(bytes),
                 bytes[off::U16STRING_short_size]};
}

// Whether `view` is Edge's toolbar chat button (EdgeCopilotToolbarLabelButton).
inline bool IsCopilotButton(void* view) {
  const uintptr_t vtable = VtableRva(view);
  for (const uintptr_t candidate : rva::CopilotButton_vftables) {
    if (vtable == candidate) {
      return true;
    }
  }
  return false;
}
// The view a ToolbarButton's context-menu-controller subobject belongs to.
inline void* ContextMenuControllerButton(void* controller) {
  return reinterpret_cast<uint8_t*>(controller) -
         off::TOOLBARBUTTON_contextMenuController;
}
// The ui::SimpleMenuModel of a chat button's right-click menu, or null.
inline void* ChatButtonContextMenuModel(void* button) {
  void* contextMenu = Field<void*>(button, off::TOOLBARBUTTON_contextMenu);
  return contextMenu ? Field<void*>(contextMenu, off::TOOLBARMENU_model)
                     : nullptr;
}
// The ui::SimpleMenuModel embedded in a page context menu (RenderViewContextMenu).
inline void* PageContextMenuModel(void* contextMenu) {
  return reinterpret_cast<uint8_t*>(contextMenu) + off::RVCM_menuModel;
}
// The behaviour object a chat button holds.
inline void* CopilotButtonAction(void* button) {
  return Field<void*>(button, off::DISCOVERBUTTON_action);
}
// The label UpdateLabelFromController last set ("Chat" unless a nudge).
inline U16View CopilotButtonCachedLabel(void* button) {
  return U16StringView(reinterpret_cast<uint8_t*>(button) +
                       off::COPILOTBUTTON_label);
}
inline void CopilotButtonUpdateLabel(void* button) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::CopilotButton_UpdateLabelFromController))(
      button);
}
// Re-read the action's images (and the button's theme colours).
inline void CopilotButtonRefreshImages(void* button) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::CopilotButton_OnThemeChanged))(button);
}

inline void LabelButtonSetText(void* button, U16View text) {
  using Fn = void(__fastcall*)(void*, const U16View*);
  reinterpret_cast<Fn>(At(rva::LabelButton_SetText))(button, &text);
}

// The accessible name of `view`, from UTF-8. SetName takes its u16string by
// value, so the callee owns (and frees) the one built here.
inline void ViewSetAccessibleName(void* view, U16String* name) {
  using AccessibilityFn = void*(__fastcall*)(void*);
  using SetNameFn = void(__fastcall*)(void*, U16String*);
  reinterpret_cast<SetNameFn>(At(rva::ViewAccessibility_SetName))(
      reinterpret_cast<AccessibilityFn>(At(rva::View_GetViewAccessibility))(
          view),
      name);
}
inline void ViewSetAccessibleName(void* view, const char* utf8) {
  U16String name{};
  Utf8ToUtf16(&name, utf8);
  ViewSetAccessibleName(view, &name);
}

// Edge's localized string for message id `messageId`, built into `out`
// (uninitialised storage the caller owns; short strings hold nothing on the
// heap, which every title used here is).
inline void LocalizedString(U16String* out, int messageId) {
  using Fn = void*(__fastcall*)(void*, int);
  reinterpret_cast<Fn>(At(rva::L10n_GetStringUTF16))(out, messageId);
}
// The same with one argument: message `messageId` with `$1` replaced by `arg`.
inline void LocalizedString(U16String* out, int messageId,
                            const U16String* arg) {
  using Fn = void*(__fastcall*)(void*, int, const void*);
  reinterpret_cast<Fn>(At(rva::L10n_GetStringFUTF16))(out, messageId, arg);
}
// views::View::SetTooltipText(view, const std::u16string&).
inline void ViewSetTooltip(void* view, const U16String* text) {
  using Fn = void(__fastcall*)(void*, const void*);
  reinterpret_cast<Fn>(At(rva::View_SetTooltipText))(view, text);
}

// Whether `size` bytes at `address` can be read.
inline bool IsReadable(const void* address, size_t size) {
  MEMORY_BASIC_INFORMATION info{};
  const bool isQueried = VirtualQuery(address, &info, sizeof(info)) != 0;
  const uint8_t* end = reinterpret_cast<const uint8_t*>(address) + size;
  const uint8_t* regionEnd =
      reinterpret_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
  const bool isMapped = info.State == MEM_COMMIT &&
                        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
  return isQueried && isMapped && end <= regionEnd;
}

// The EdgeToolbarView that owns chat button `button`, or null: the ancestor
// whose chat-button field is the button.
inline void* ToolbarOfChatButton(void* button) {
  for (void* view = ViewParent(button); view; view = ViewParent(view)) {
    const void* field =
        reinterpret_cast<uint8_t*>(view) + off::EDGETOOLBAR_chatButton;
    const bool isOwner =
        IsReadable(field, sizeof(void*)) &&
        Field<void*>(view, off::EDGETOOLBAR_chatButton) == button;
    if (isOwner) {
      return view;
    }
  }
  return nullptr;
}

// Build `toolbar`'s chat button again, as Edge does for a new window (so the
// chevron's container if Edge now says so), and take the old one out of the
// view tree. The old button is detached, never deleted: other Edge objects may
// still point at it, and a view nobody draws is harmless. False when Edge built
// nothing.
inline bool RebuildChatButton(void* toolbar) {
  void* oldButton = Field<void*>(toolbar, off::EDGETOOLBAR_chatButton);
  void* oldParent = ViewParent(oldButton);
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(
      At(rva::EdgeToolbarView_CreateDiscoverButtonIfNecessary))(
      reinterpret_cast<uint8_t*>(toolbar) + off::EDGETOOLBAR_discoverBase);
  const bool isRebuilt =
      Field<void*>(toolbar, off::EDGETOOLBAR_chatButton) != oldButton;
  if (isRebuilt && oldParent) {
    ViewRemoveChild(oldParent, oldButton);
  }
  return isRebuilt;
}

// Whether `view` is the chevron beside the chat pill
// (EdgeCopilotDropdownMenuButton).
inline bool IsCopilotDropdownButton(void* view) {
  const uintptr_t vtable = VtableRva(view);
  for (const uintptr_t candidate : rva::CopilotDropdownButton_vftables) {
    if (vtable == candidate) {
      return true;
    }
  }
  return false;
}

// Draw `vectorIcon` at `dipSize` in `color` (ARGB) into the gfx::ImageSkia at
// `outImage` (uninitialised storage the caller owns).
inline void CreateVectorIcon(void* outImage, const void* vectorIcon,
                             int dipSize, uint32_t color) {
  using Fn = void*(__fastcall*)(void*, const void*, int, uint32_t);
  reinterpret_cast<Fn>(At(rva::Gfx_CreateVectorIcon))(outImage, vectorIcon,
                                                      dipSize, color);
}

// A views::WebView for `browserContext` (its WebContents is created on first
// load, in that profile's own storage), not yet parented.
inline void* NewWebView(void* browserContext) {
  void* webView = NewView(off::WebView_size);
  using CtorFn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<CtorFn>(At(rva::WebView_ctor))(webView, browserContext);
  return webView;
}
// Load `url` (UTF-8) in `webView`, creating its WebContents.
inline void WebViewLoadUrl(void* webView, const char* url) {
  const U8View urlView{url, std::strlen(url)};
  alignas(16) uint8_t gurl[off::GURL_size] = {};
  using GurlFn = void*(__fastcall*)(void*, const void*);
  reinterpret_cast<GurlFn>(At(rva::GURL_ctor_string))(gurl, &urlView);
  constexpr int kNoHttpsUpgradePolicy = 0;
  // An empty base::Location: it only labels the WebContents creation.
  alignas(8) uint8_t location[off::Location_size] = {};
  using LoadFn = void(__fastcall*)(void*, const void*, int, void*);
  reinterpret_cast<LoadFn>(At(rva::WebView_LoadInitialURL))(
      webView, gurl, kNoHttpsUpgradePolicy, location);
  using DtorFn = void(__fastcall*)(void*);
  reinterpret_cast<DtorFn>(At(rva::GURL_dtor))(gurl);
}
// Destroy an unparented WebView and the WebContents it owns.
inline void DeleteWebView(void* webView) {
  constexpr int kDeleteSelf = 1;
  using DtorFn = void*(__fastcall*)(void*, int);
  reinterpret_cast<DtorFn>(At(rva::WebView_ScalarDeletingDtor))(webView,
                                                                kDeleteSelf);
}
inline void ViewRequestFocus(void* view) {
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::View_RequestFocus))(view);
}

// --- reading a tab, and running a script in a page ---------------------------
// (docs/re/gemini-in-chrome.md section 8)

// The WebContents a views::WebView shows (null before its first load).
inline void* WebViewContents(void* webView) {
  return Field<void*>(webView, off::CWV_web_contents);
}

// A libc++ std::string's characters (the same layout as the u16string above).
inline std::string StdStringText(const void* string) {
  auto* bytes = static_cast<const uint8_t*>(string);
  const bool isLong =
      (bytes[off::U16STRING_short_size] & off::U16STRING_long_flag) != 0;
  if (isLong) {
    return std::string(
        *reinterpret_cast<const char* const*>(bytes),
        *reinterpret_cast<const size_t*>(bytes + off::U16STRING_long_size));
  }
  return std::string(reinterpret_cast<const char*>(bytes),
                     bytes[off::U16STRING_short_size]);
}

inline std::string Utf16ToUtf8(U16View text) {
  if (text.length == 0) {
    return std::string();
  }
  const int length = static_cast<int>(text.length);
  const int byteCount = WideCharToMultiByte(
      CP_UTF8, 0, reinterpret_cast<const wchar_t*>(text.data), length, nullptr,
      0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(byteCount), '\0');
  WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(text.data),
                      length, utf8.data(), byteCount, nullptr, nullptr);
  return utf8;
}

// The page title and last committed URL of `webContents`, in UTF-8.
inline std::string TabTitle(void* webContents) {
  using TitleFn = const void*(__fastcall*)(void*);
  const void* title =
      reinterpret_cast<TitleFn>(At(rva::WebContentsImpl_GetTitle))(webContents);
  return title ? Utf16ToUtf8(U16StringView(title)) : std::string();
}
inline std::string TabUrl(void* webContents) {
  using SpecFn = const void*(__fastcall*)(const void*);
  const void* url = LastCommittedUrl(webContents);
  const void* spec =
      url ? reinterpret_cast<SpecFn>(At(rva::GURL_spec))(url) : nullptr;
  return spec ? StdStringText(spec) : std::string();
}

inline void* PrimaryMainFrame(void* webContents) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::WebContentsImpl_GetPrimaryMainFrame))(
      webContents);
}

// Whether the tab's page has a running renderer: false for a discarded
// (asleep) tab, whose page cannot run a script.
inline bool IsPageLive(void* webContents) {
  const void* frame = PrimaryMainFrame(webContents);
  using Fn = bool(__fastcall*)(const void*);
  return frame && reinterpret_cast<Fn>(
                      At(rva::RenderFrameHostImpl_IsRenderFrameLive))(frame);
}

// What a finished script handed back: its string result, or nothing when the
// script returned something else (or threw).
struct ScriptResult {
  bool isString = false;
  std::string text;
};
using ScriptDone = std::function<void(const ScriptResult&)>;

// A hand-made base::OnceCallback<void(base::Value)>: a BindState we lay out
// ourselves (Edge only ever calls its invoke and destructor slots), holding the
// std::function to run. Edge destroys it exactly once, run or not.
struct ScriptCallbackState {
  alignas(8) uint8_t bindStateBase[off::BINDSTATE_first_bound_argument];
  ScriptDone done;
};
// Callbacks Edge still holds. DLL unload waits for this to reach zero: a reply
// that arrived after the DLL was freed would jump into unmapped code.
inline std::atomic<int> g_liveScriptCallbacks{0};

inline void __fastcall InvokeScriptCallback(void* bindState, void* value) {
  auto* state = static_cast<ScriptCallbackState*>(bindState);
  ScriptResult result;
  const uint8_t variantIndex =
      value ? Field<uint8_t>(value, off::VALUE_variant_index) : 0;
  result.isString = variantIndex == off::VALUE_string_index;
  if (result.isString) {
    result.text = StdStringText(value);
  }
  state->done(result);
}
inline void __fastcall DestroyScriptCallback(const void* bindState) {
  delete static_cast<const ScriptCallbackState*>(bindState);
  g_liveScriptCallbacks--;
}

// Brotli-decompress `size` bytes at `data` with Edge's own decoder into `out`,
// which must hold `decodedSize` bytes (a pak resource says its size up front).
inline bool BrotliDecompress(const uint8_t* data, size_t size,
                             size_t decodedSize, std::string* out) {
  constexpr int kBrotliDecoderSuccess = 1;
  out->resize(decodedSize);
  size_t written = decodedSize;
  using Fn = int(__fastcall*)(size_t, const uint8_t*, size_t*, uint8_t*);
  const int result = reinterpret_cast<Fn>(At(rva::BrotliDecoderDecompress))(
      size, data, &written, reinterpret_cast<uint8_t*>(out->data()));
  out->resize(written);
  return result == kBrotliDecoderSuccess;
}

// Gunzip `size` bytes at `data` with Edge's zlib into `out`, which must hold
// `decodedSize` bytes (the size a gzip stream ends with).
inline bool GzipDecompress(const uint8_t* data, size_t size, size_t decodedSize,
                           std::string* out) {
  constexpr int kZlibOk = 0;
  out->resize(decodedSize);
  uint32_t written = static_cast<uint32_t>(decodedSize);  // uLongf is 32 bits
  using Fn = int(__fastcall*)(uint8_t*, uint32_t*, const uint8_t*, uint32_t);
  const int result = reinterpret_cast<Fn>(At(rva::GzipUncompressHelper))(
      reinterpret_cast<uint8_t*>(out->data()), &written, data,
      static_cast<uint32_t>(size));
  out->resize(written);
  return result == kZlibOk;
}

// Whether every script callback has been destroyed within `timeoutMs`.
inline bool WaitForScriptCallbacks(DWORD timeoutMs) {
  constexpr DWORD kStepMs = 20;
  for (DWORD waited = 0; waited < timeoutMs; waited += kStepMs) {
    if (g_liveScriptCallbacks == 0) {
      return true;
    }
    Sleep(kStepMs);
  }
  return g_liveScriptCallbacks == 0;
}

// Free a u16string we made with Utf8ToUtf16 (its text is on Edge's heap when
// it outgrew the inline buffer).
inline void FreeU16String(U16String* string) {
  auto* bytes = reinterpret_cast<uint8_t*>(string);
  const bool isLong =
      (bytes[off::U16STRING_short_size] & off::U16STRING_long_flag) != 0;
  if (isLong) {
    EdgeDelete(*reinterpret_cast<void**>(bytes));
  }
}

// Run `utf8Script` in `webContents`' main frame, in our isolated world, and
// pass `done` the script's string result on the UI thread later (pass an empty
// `done` to not wait). False if the page has no live renderer to run it.
inline bool RunScript(void* webContents, const std::string& utf8Script,
                      ScriptDone done) {
  const void* frame =
      IsPageLive(webContents) ? PrimaryMainFrame(webContents) : nullptr;
  if (!frame) {
    return false;
  }
  const void* callbackSlot = nullptr;
  if (done) {
    auto* state = new ScriptCallbackState{};
    using BaseCtor = void*(__fastcall*)(void*, void*, void*);
    reinterpret_cast<BaseCtor>(At(rva::BindStateBase_ctor))(
        state, reinterpret_cast<void*>(InvokeScriptCallback),
        reinterpret_cast<void*>(DestroyScriptCallback));
    state->done = std::move(done);
    g_liveScriptCallbacks++;
    callbackSlot = state;
  }
  U16String script{};
  const U8View scriptView{utf8Script.data(), utf8Script.size()};
  Utf8ToUtf16(&script, &scriptView);
  using ExecuteFn =
      void(__fastcall*)(const void*, const U16String*, const void**, int);
  reinterpret_cast<ExecuteFn>(
      At(rva::RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld))(
      frame, &script, &callbackSlot, off::kScriptWorldId);
  FreeU16String(&script);
  return true;
}

// --- the native find bar -------------------------------------------------------

// What the bar's text field holds now (a view of the field's own storage, valid
// until the field next changes).
inline U16View FindBarText(void* findBarView) {
  U16View text{nullptr, 0};
  using Fn = void*(__fastcall*)(void*, U16View*);
  reinterpret_cast<Fn>(At(rva::Textfield_GetText))(
      Field<void*>(findBarView, off::FINDBARVIEW_textfield), &text);
  return text;
}

// Show "ordinal/matches" in the bar's count label and enable its arrow buttons,
// as Edge does for its own search's result. `findText` must be what the text
// field holds: the bar rewrites the field when they differ.
inline void FindBarShowResult(void* findBarView, int matches, int ordinal,
                              U16View findText) {
  static int s_requestId = 0;
  uint8_t details[off::FINDDETAILS_size] = {};
  // The bar skips a result equal to the one it last showed; ours may follow a
  // status line it knows nothing about, so every result is a new one.
  s_requestId++;
  std::memcpy(details + off::FINDDETAILS_request_id, &s_requestId,
              sizeof(s_requestId));
  std::memcpy(details + off::FINDDETAILS_matches, &matches, sizeof(matches));
  std::memcpy(details + off::FINDDETAILS_active_ordinal, &ordinal,
              sizeof(ordinal));
  details[off::FINDDETAILS_is_final] = 1;
  // A read-only std::u16string in its long form {data, size, capacity|flag}.
  struct LongU16String {
    const char16_t* data;
    size_t size;
    size_t capacityAndFlag;
  };
  const LongU16String text{
      findText.data, findText.length,
      (findText.length + 1) |
          (size_t{off::U16STRING_long_flag} << (8 * (sizeof(size_t) - 1)))};
  using Fn = void(__fastcall*)(void*, const void*, const void*);
  reinterpret_cast<Fn>(At(rva::FindBarView_UpdateForResult))(findBarView,
                                                             details, &text);
}

// The bar's count label (a views::Label).
inline void* FindBarLabel(void* findBarView) {
  return Field<void*>(findBarView, off::FINDBARVIEW_match_count_label);
}

// Whether `keyEvent` (a ui::KeyEvent) is Enter being pressed.
inline bool IsEnterPressed(const void* keyEvent) {
  auto* bytes = static_cast<const uint8_t*>(keyEvent);
  int32_t type = 0;
  std::memcpy(&type, bytes + off::KEYEVENT_type, sizeof(type));
  const bool isPressed = type == off::KEYEVENT_type_pressed;
  const bool isEnter = bytes[off::KEYEVENT_key_code] == off::KEYEVENT_key_enter;
  return isPressed && isEnter;
}

// The FindBarView behind the TextfieldController its key handler is called on.
inline void* FindBarViewOfKeyController(void* controller) {
  return static_cast<uint8_t*>(controller) -
         off::FINDBARVIEW_textfield_controller;
}

// Put `text` in the bar's count label in place of "n/m" (a short status).
inline void FindBarSetLabel(void* findBarView, U16View text) {
  void* label = FindBarLabel(findBarView);
  using Fn = void(__fastcall*)(void*, const U16View*);
  reinterpret_cast<Fn>(VirtualFunction(
      label, off::FINDBARLABEL_set_text_vtable_offset))(label, &text);
}

// --- the bar's option toggles -----------------------------------------------------

// The options view (a column of toggles), or null before the bar first needed it.
inline void* FindOptionsView(void* findBarView) {
  return Field<void*>(findBarView, off::FINDBARVIEW_options_view);
}

// The toggle built for `id`, or null. A walk down the options view's map, as
// Edge's own AddFindInPageOptionChildView does.
inline void* FindToggle(void* findBarView, int toggleId) {
  void* optionsView = FindOptionsView(findBarView);
  if (!optionsView) {
    return nullptr;
  }
  auto* node = Field<uint8_t*>(optionsView, off::FINDOPTIONS_toggle_map_root);
  while (node) {
    const int key = Field<int>(node, off::MAPNODE_key);
    if (toggleId == key) {
      return Field<void*>(node, off::MAPNODE_value);
    }
    node = Field<uint8_t*>(
        node, toggleId < key ? off::MAPNODE_left : off::MAPNODE_right);
  }
  return nullptr;
}

// The FindBarView a toggle sits in (through its FindBarHost), or null.
inline void* FindBarViewOfToggle(void* toggle) {
  void* host = Field<void*>(toggle, off::FINDTOGGLE_host);
  return host ? Field<void*>(host, off::FINDBARHOST_view) : nullptr;
}

inline int FindToggleId(void* toggle) {
  return Field<int>(toggle, off::FINDTOGGLE_id);
}

inline bool FindToggleIsOn(void* toggle) {
  const void* const button = Field<void*>(toggle, off::FINDTOGGLE_button);
  using Fn = bool(__fastcall*)(const void*);
  return button &&
         reinterpret_cast<Fn>(At(rva::EdgeToggleButton_GetIsOn))(button);
}

// Move the switch alone: Edge's SetToggleState would also write the id into its
// own find parameters, which means nothing for our two.
inline void FindToggleSetOn(void* toggle, bool isOn) {
  const void* const button = Field<void*>(toggle, off::FINDTOGGLE_button);
  using Fn = void(__fastcall*)(const void*, bool);
  if (button) {
    reinterpret_cast<Fn>(At(rva::EdgeToggleButton_SetIsOn))(button, isOn);
  }
}

inline void ViewSetEnabled(void* view, bool isEnabled) {
  using Fn = void(__fastcall*)(void*, bool);
  reinterpret_cast<Fn>(At(rva::View_SetEnabled))(view, isEnabled);
}

// Enable or grey out a whole toggle: the title and the switch (a disabled
// switch takes no click).
inline void FindToggleSetEnabled(void* toggle, bool isEnabled) {
  ViewSetEnabled(Field<void*>(toggle, off::FINDTOGGLE_label), isEnabled);
  ViewSetEnabled(Field<void*>(toggle, off::FINDTOGGLE_button), isEnabled);
}

inline void FindToggleSetTitle(void* toggle, U16View title) {
  const void* const label = Field<void*>(toggle, off::FINDTOGGLE_label);
  using Fn = void(__fastcall*)(const void*, const U16View*);
  reinterpret_cast<Fn>(At(rva::Label_SetText))(label, &title);
}

// Build toggle `id` at the end of the options view, sized like Edge's own
// (whose width it reads off one that exists). Null when there is no options view
// or no toggle of Edge's to size by.
inline void* AddFindToggle(void* findBarView, int toggleId) {
  const void* const optionsView = FindOptionsView(findBarView);
  void* sibling = nullptr;
  for (const int siblingId :
       {off::FINDTOGGLE_id_match_case, off::FINDTOGGLE_id_whole_word,
        off::FINDTOGGLE_id_diacritics}) {
    if (!sibling) {
      sibling = FindToggle(findBarView, siblingId);
    }
  }
  if (!optionsView || !sibling) {
    return nullptr;
  }
  using Fn = void(__fastcall*)(const void*, int, int, void*);
  reinterpret_cast<Fn>(
      At(rva::FindOptionsControlView_AddFindInPageOptionChildView))(
      optionsView, toggleId, Field<int>(sibling, off::FINDTOGGLE_width),
      Field<void*>(findBarView, off::FINDBARVIEW_host));
  return FindToggle(findBarView, toggleId);
}

// The options view grew or shrank: lay it out again and let the bar re-measure.
inline void FindBarRelayoutOptions(void* findBarView) {
  InvalidateLayout(FindOptionsView(findBarView));
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::FindBarView_PreferredSizeChanged))(findBarView);
}

}  // namespace fixedge::edge
