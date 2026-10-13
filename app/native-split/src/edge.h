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

}  // namespace fixedge::edge
