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

inline void* At(uintptr_t rva) { return reinterpret_cast<void*>(g_base + rva); }

// Overwrite one code byte (reversibly) — used to lift the index<2 guard.
inline uint8_t PatchByte(uintptr_t rva, uint8_t value) {
  void* addr = At(rva);
  DWORD old = 0;
  uint8_t prev = *reinterpret_cast<uint8_t*>(addr);
  if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &old)) {
    *reinterpret_cast<uint8_t*>(addr) = value;
    VirtualProtect(addr, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, 1);
  }
  return prev;  // caller keeps this to restore on unload
}

// Overwrite `n` code bytes with 0x90 (NOP), saving the originals into `saved`
// (reversibly) — used to neutralize a 2-pane-only CHECK so a grown split survives.
inline void PatchNop(uintptr_t rva, size_t n, uint8_t* saved) {
  void* addr = At(rva);
  DWORD old = 0;
  auto* p = reinterpret_cast<uint8_t*>(addr);
  if (VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old)) {
    for (size_t i = 0; i < n; ++i) { saved[i] = p[i]; p[i] = 0x90; }
    VirtualProtect(addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
  }
}
inline void Unpatch(uintptr_t rva, size_t n, const uint8_t* saved) {
  void* addr = At(rva);
  DWORD old = 0;
  auto* p = reinterpret_cast<uint8_t*>(addr);
  if (VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old)) {
    for (size_t i = 0; i < n; ++i) p[i] = saved[i];
    VirtualProtect(addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
  }
}

// Typed lvalue at obj+off. The one place raw pointer arithmetic on Edge objects
// lives; callers read/write fields by name via the Mcv*/Container* helpers below.
template <class T>
inline T& Field(void* obj, uintptr_t off) {
  return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + off);
}

// --- Edge functions, each resolved from g_base and typed exactly once ---------

inline void* NewView(size_t size) {
  using Fn = void*(__fastcall*)(size_t);
  return reinterpret_cast<Fn>(At(rva::CheckedOperatorNew3))(size);
}

inline void ConstructContainer(void* container, void* browser_view) {
  using Fn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::EdgeContentsContainerView_ctor))(container, browser_view);
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

inline void SetViewVisible(void* view, bool visible) {
  using Fn = void(__fastcall*)(void*, bool);
  reinterpret_cast<Fn>(At(rva::View_SetVisible))(view, visible);
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
// `view_slot` -> View*, `visible_slot` -> bool, `rect_slot` -> gfx::Rect (our Rect).
inline void EmplaceChildLayout(void* vector, void* view_slot, void* visible_slot, void* rect_slot) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::ChildLayouts_emplace_back))(vector, view_slot, visible_slot, rect_slot);
}

// Attach a WebContents to a ContentsWebView (the real render path).
inline void ContentsWebViewSetContents(void* contents_web_view, void* web_contents) {
  using Fn = void*(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(At(rva::ContentsWebView_SetWebContents))(contents_web_view, web_contents);
}

// Force a (background) WebContents to the visible state so the pane paints.
inline void WebContentsWasShown(void* web_contents) {
  using Fn = void*(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::WebContents_WasShown))(web_contents);
}

// --- native split data-model helpers ---------------------------------------

// Edge's non-View operator new (so a buffer we hand to a split primitive can be
// reallocated/freed by Edge's own allocator).
inline void* EdgeNew(size_t size) {
  using Fn = void*(__fastcall*)(size_t);
  return reinterpret_cast<Fn>(At(rva::EdgeOperatorNew))(size);
}
// base::Token::CreateRandom(out 16-byte Token*) — a valid split id.
inline void CreateRandomToken(void* out16) {
  using Fn = void*(__fastcall*)(void*);
  reinterpret_cast<Fn>(At(rva::TokenCreateRandom))(out16);
}

// The split primitives' index argument: {int* data; size_t size; size_t cap}
// (NOT libc++ vector) — confirmed from the caller storing rodata {1,1} at +8.
struct SplitIndexVec {
  int* data;
  size_t size;
  size_t cap;
};

// Build a SplitIndexVec of `count` tab indices in an Edge-allocated buffer.
inline SplitIndexVec MakeIndexVec(const int* indices, size_t count) {
  int* buf = static_cast<int*>(EdgeNew(count * sizeof(int)));
  for (size_t i = 0; i < count; ++i) buf[i] = indices[i];
  return SplitIndexVec{buf, count, count};
}

// TabStripModel members + the split primitives (docs/re/split-grow-impl.md).
inline void* RootCollection(void* tsm) { return Field<void*>(tsm, off::TSM_collection); }
inline void* ActiveTab(void* tsm) { return Field<void*>(tsm, off::TSM_active_tab); }

// A TabModel virtual: resolve a vtable slot (byte offset) and call it.
inline bool TabIsSplit(void* tab) {
  void** vtbl = *reinterpret_cast<void***>(tab);
  using Fn = bool(__fastcall*)(void*);
  return reinterpret_cast<Fn>(vtbl[off::TABMODEL_vtbl_IsSplit / 8])(tab);
}
inline void TabGetSplit(void* tab, void* out_opt /*24B: Token@0, bool@0x10*/) {
  void** vtbl = *reinterpret_cast<void***>(tab);
  using Fn = void(__fastcall*)(void*, void*);
  reinterpret_cast<Fn>(vtbl[off::TABMODEL_vtbl_GetSplit / 8])(tab, out_opt);
}

inline void* GetSplitData(void* tsm, void* id16) {
  using Fn = void*(__fastcall*)(void*, void*);
  return reinterpret_cast<Fn>(At(rva::GetSplitData))(tsm, id16);
}
inline void GetTabsAndIndicesInSplit(void* tsm, void* out_vec24, void* id16) {
  using Fn = void*(__fastcall*)(void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::GetTabsAndIndicesInSplit))(tsm, out_vec24, id16);
}
inline void* GetTabAtIndexRecursive(void* root_collection, int index) {
  using Fn = void*(__fastcall*)(void*, int);
  return reinterpret_cast<Fn>(At(rva::GetTabAtIndexRecursive))(root_collection, index);
}
// GetIndexOfTabRecursive(rcx=collection, rdx=out{size_t index; bool found}, r8=tab).
inline int GetIndexOfTabRecursive(void* root_collection, void* tab_model) {
  struct { uint64_t index; uint8_t found; uint8_t pad[7]; } out = {};
  using Fn = void(__fastcall*)(void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::GetIndexOfTabRecursive))(root_collection, &out, tab_model);
  return out.found ? static_cast<int>(out.index) : -1;
}
inline void RemoveSplitImpl(void* tsm, void* id16, int flag) {
  using Fn = void*(__fastcall*)(void*, void*, int);
  reinterpret_cast<Fn>(At(rva::TabStripModel_RemoveSplitImpl))(tsm, id16, flag);
}
// 7 args: 4 register + 3 stack ([rsp+0x20/0x28/0x30]).
inline void AddToSplitImpl(void* tsm, void* out_id16, void* id16, SplitIndexVec* indices,
                           int anchor, void* visual16, int flag) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*, int, void*, int);
  reinterpret_cast<Fn>(At(rva::TabStripModel_AddToSplitImpl))(tsm, out_id16, id16, indices, anchor,
                                                              visual16, flag);
}
// AddToNewSplit(tsm, out base::Token*, SplitIndexVec*, SplitTabVisualData*) — creates
// a 2-pane split (its size==1 branch adds the active tab).
inline void AddToNewSplit(void* tsm, void* out_id16, SplitIndexVec* indices, void* visual16) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*);
  reinterpret_cast<Fn>(At(rva::TabStripModel_AddToNewSplit))(tsm, out_id16, indices, visual16);
}
// UpdateTabInSplit(tsm, anchor=last split member TabModel*, int newTabIndex, int flag)
// — Edge's own "add a tab to the current split": runs the whole remove+move+add
// transaction internally (how OpenTabInCurrentSplitWindow adds a pane).
inline void UpdateTabInSplit(void* tsm, void* anchor_member, int new_tab_index, int flag) {
  using Fn = void*(__fastcall*)(void*, void*, int, int);
  reinterpret_cast<Fn>(At(rva::TabStripModel_UpdateTabInSplit))(tsm, anchor_member, new_tab_index,
                                                                flag);
}

// HWND -> aura::Window* -> BrowserView*, the bridge to reach a browser from a
// Win32 window handle (Chromium is Aura; GetBrowserViewForNativeWindow wants the
// aura window, not the HWND).
inline void* AuraWindowForHWND(void* hwnd) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::GetContentWindowForHWND))(hwnd);
}
inline void* BrowserViewForAura(void* aura_window) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(At(rva::BrowserView_ForNativeWindow))(aura_window);
}

// TabStripModel::GetWebContentsAt — bounds-checked by the callee; caller must
// still pass 0 <= index < TabCount or it aborts.
inline void* TabWebContentsAt(void* tab_strip_model, int index) {
  using Fn = void*(__fastcall*)(void*, int);
  return reinterpret_cast<Fn>(At(rva::TabStripModel_GetWebContentsAt))(tab_strip_model, index);
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
// ui::SimpleMenuModel::AddItem(model /*rcx*/, int command_id /*edx*/, &u16string /*r8*/).
inline void MenuAddItem(void* model, int command_id, const U16String* label) {
  using Fn = void*(__fastcall*)(void*, int, const void*);
  reinterpret_cast<Fn>(At(rva::SimpleMenuModel_AddItem))(model, command_id, label);
}

// --- MultiContentsView field accessors (named, typed, one place each) ---------

// The container vector OBJECT begins at this address (pass to ContainersEmplaceBack).
inline void* McvContainerVector(void* mcv) {
  return reinterpret_cast<uint8_t*>(mcv) + off::MCV_containers_data;
}
// The vector's begin pointer: an array of ContentsContainerView*.
inline void** McvContainers(void* mcv) { return Field<void**>(mcv, off::MCV_containers_data); }
inline void* McvContainerAt(void* mcv, int i) { return McvContainers(mcv)[i]; }
inline size_t McvCount(void* mcv) { return Field<size_t>(mcv, off::MCV_containers_count); }
inline void* McvBrowserView(void* mcv) { return Field<void*>(mcv, off::MCV_browser_view); }
inline uint32_t McvOrientation(void* mcv) { return Field<uint32_t>(mcv, off::MCV_orientation); }
inline size_t McvChildInsertIndex(void* mcv) { return Field<size_t>(mcv, off::MCV_child_insert_idx); }

// CalculateProposedLayout runs on the LayoutManagerBase subobject at MCV+0x340.
inline void* McvFromLayoutBase(void* layout_base) {
  return reinterpret_cast<uint8_t*>(layout_base) - off::MCV_layout_base;
}
inline void* BrowserViewMcv(void* browser_view) {
  return Field<void*>(browser_view, off::BV_multi_contents_view);
}

// A container's ContentsWebView (the pane's web view).
inline void* ContainerWebView(void* container) {
  return Field<void*>(container, off::CCV_contents_web_view);
}

// --- Browser / TabStripModel field accessors ---------------------------------
inline void* BrowserOf(void* browser_view) { return Field<void*>(browser_view, off::BV_browser); }
inline void* TabStripModelOf(void* browser) { return Field<void*>(browser, off::BROWSER_tab_strip_model); }
inline int TabCount(void* tab_strip_model) {
  void* collection = Field<void*>(tab_strip_model, off::TSM_collection);
  return collection ? Field<int32_t>(collection, off::TABCOLL_count) : 0;
}

// How many panes this MCV should show: the active tab's split member count
// (>=2), or 1 when the active tab isn't in a split (collapsed / normal). The
// data model is the single source of truth; the container vector is a cache that
// only grows, so surplus containers are hidden rather than destroyed.
inline int McvActiveSplitPanes(void* mcv) {
  void* bv = McvBrowserView(mcv);
  void* tsm = bv ? TabStripModelOf(BrowserOf(bv)) : nullptr;
  if (!tsm) return 1;
  void* active = ActiveTab(tsm);
  if (!active || !TabIsSplit(active)) return 1;
  struct { uint8_t token[16]; uint8_t present; uint8_t pad[7]; } opt = {};
  TabGetSplit(active, &opt);
  if (!opt.present) return 1;
  uint64_t vec[3] = {};  // {data, size, cap}; size = member count
  GetTabsAndIndicesInSplit(tsm, vec, opt.token);
  const int n = vec[0] ? static_cast<int>(vec[1]) : 1;
  return n < 1 ? 1 : n;
}

// TabStripModel from a top-level Win32 browser window handle (no split needed).
inline void* TsmForWindow(void* hwnd) {
  void* aura = AuraWindowForHWND(hwnd);
  if (!aura) return nullptr;
  void* bv = BrowserViewForAura(aura);
  return bv ? TabStripModelOf(BrowserOf(bv)) : nullptr;
}

// MultiContentsView from a top-level Win32 browser window handle.
inline void* McvForWindow(void* hwnd) {
  void* aura = AuraWindowForHWND(hwnd);
  if (!aura) return nullptr;
  void* bv = BrowserViewForAura(aura);
  return bv ? BrowserViewMcv(bv) : nullptr;
}

}  // namespace fixedge::edge
