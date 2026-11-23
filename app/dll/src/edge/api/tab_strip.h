// tab_strip.h — a window's TabStripModel: its tabs, the active one, a tab's
// page, and closing, activating and moving a tab.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// TabStripModel members + the split primitives (docs/re/split-grow-impl.md).
inline void* RootCollection(void* tsm) {
  return Field<void*>(tsm, edge155::off::TSM_collection);
}
inline void* ActiveTab(void* tsm) {
  return Field<void*>(tsm, edge155::off::TSM_active_tab);
}

// TabModel virtuals, called through the tab's vtable.
inline void* TabContents(void* tab) {
  using Fn = void*(__fastcall*)(void*);
  return reinterpret_cast<Fn>(
      VirtualFunction(tab, edge155::off::TABMODEL_vtbl_GetContents))(tab);
}

inline void* GetTabAtIndexRecursive(void* rootCollection, int index) {
  using Fn = void*(__fastcall*)(void*, int);
  return Callable<Fn>(edge155::rva::GetTabAtIndexRecursive)(rootCollection,
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
  Callable<Fn>(edge155::rva::GetIndexOfTabRecursive)(rootCollection, &result,
                                                     tabModel);
  return result.isFound ? static_cast<int>(result.index) : -1;
}
// Close the tab at strip `index` as a user gesture (Edge's own X does this).
inline void CloseTabAt(void* tsm, int index) {
  using CloseFn = void(__fastcall*)(void*, int, uint32_t);
  Callable<CloseFn>(edge155::rva::TabStripModel_CloseWebContentsAt)(
      tsm, index, edge155::off::TSM_close_user_gesture);
}

// Activate the tab at strip `index` (no gesture metrics: details zeroed).
inline void ActivateTabAt(void* tsm, int index) {
  uint8_t gestureDetails[edge155::off::TSM_gesture_details_size] = {};
  using ActivateFn = void(__fastcall*)(void*, int, const void*);
  Callable<ActivateFn>(edge155::rva::TabStripModel_ActivateTabAt)(
      tsm, index, gestureDetails);
}

inline void* TabModelAt(void* tsm, int index) {
  void* root = RootCollection(tsm);
  return root && index >= 0 ? GetTabAtIndexRecursive(root, index) : nullptr;
}

// The strip index of the tab showing `webContents`, or -1.
inline int IndexOfWebContents(void* tsm, void* webContents) {
  using IndexFn = int(__fastcall*)(void*, void*);
  return Callable<IndexFn>(edge155::rva::TabStripModel_GetIndexOfWebContents)(
      tsm, webContents);
}

inline void* TabShowing(void* tsm, void* webContents) {
  return tsm ? TabModelAt(tsm, IndexOfWebContents(tsm, webContents)) : nullptr;
}

// Move the tab at strip `index` so it ends up at `toIndex`, leaving the
// selection alone.
inline void MoveTabTo(void* tsm, int index, int toIndex) {
  using MoveFn = int(__fastcall*)(void*, int, int, bool);
  Callable<MoveFn>(edge155::rva::TabStripModel_MoveWebContentsAt)(
      tsm, index, toIndex, false);
}

// TabStripModel::GetWebContentsAt — bounds-checked by the callee; caller must
// still pass 0 <= index < TabCount or it aborts.
inline void* TabWebContentsAt(void* tabStripModel, int index) {
  using Fn = void*(__fastcall*)(void*, int);
  return Callable<Fn>(edge155::rva::TabStripModel_GetWebContentsAt)(
      tabStripModel, index);
}

inline int TabCount(void* tabStripModel) {
  void* collection = Field<void*>(tabStripModel, edge155::off::TSM_collection);
  return collection ? Field<int32_t>(collection, edge155::off::TABCOLL_count)
                    : 0;
}

}  // namespace edgeplus::edge
