// edge.h — the single binding layer between our DLL and msedge.dll.
//
// SSOT / DRY: every RVA call and every struct-field access goes through here, so
// a struct offset or function signature is written exactly once. offsets.h holds
// the raw numbers; this header gives them names, types and calling conventions.
// On x64 every calling convention collapses to the one Windows ABI, so the
// `__fastcall` tags are documentation, not behaviour.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
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
// views::View::SetTooltipText(view, const std::u16string&), from UTF-8.
inline void ViewSetTooltip(void* view, const char* utf8) {
  U16String text{};
  const U8View utf8View{utf8, strlen(utf8)};
  Utf8ToUtf16(&text, &utf8View);
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
  void* browserView = McvBrowserView(mcv);
  void* tsm = browserView ? TabStripModelOf(BrowserOf(browserView)) : nullptr;
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

// MultiContentsView from a top-level Win32 browser window handle.
inline void* McvForWindow(void* hwnd) {
  void* auraWindow = AuraWindowForHWND(hwnd);
  if (!auraWindow) {
    return nullptr;
  }
  void* browserView = BrowserViewForAura(auraWindow);
  return browserView ? BrowserViewMcv(browserView) : nullptr;
}

}  // namespace fixedge::edge
