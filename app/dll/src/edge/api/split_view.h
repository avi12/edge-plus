// split_view.h — Edge's split view, the MultiContentsView: its pane containers
// and their pages, its orientation and resize area, and how many panes the
// active split has.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/api/browser.h"
#include "edge/api/split_model.h"
#include "edge/api/tab_strip.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline void ConstructContainer(void* container, void* browserView) {
  using Fn = void*(__fastcall*)(void*, void*);
  Callable<Fn>(edge155::rva::EdgeContentsContainerView_ctor)(container,
                                                             browserView);
}

// Appends a raw_ptr<ContentsContainerView> to the MCV container vector; `slot`
// points to an 8-byte stack temp holding the container pointer.
inline void ContainersEmplaceBack(void* vector, void* slot) {
  using Fn = void*(__fastcall*)(void*, void*);
  Callable<Fn>(edge155::rva::Containers_emplace_back)(vector, slot);
}

// Re-draw each pane's border/overlay from MCV_active_index (loops every container).
inline void McvUpdateBorders(void* mcv) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::MultiContentsView_UpdateContentsBorderAndOverlay)(
      mcv);
}

inline void ContentsWebViewSetContents(void* contentsWebView,
                                       void* webContents) {
  using Fn = void*(__fastcall*)(void*, void*);
  Callable<Fn>(edge155::rva::ContentsWebView_SetWebContents)(contentsWebView,
                                                             webContents);
}

// The container vector OBJECT begins at this address (pass to ContainersEmplaceBack).
inline void* McvContainerVector(void* mcv) {
  return reinterpret_cast<uint8_t*>(mcv) + edge155::off::MCV_containers_data;
}
// The vector's begin pointer: an array of ContentsContainerView*.
inline void** McvContainers(void* mcv) {
  return Field<void**>(mcv, edge155::off::MCV_containers_data);
}
inline void* McvContainerAt(void* mcv, int index) {
  return McvContainers(mcv)[index];
}
inline size_t McvCount(void* mcv) {
  return Field<uint64_t>(mcv, edge155::off::MCV_containers_count);
}
inline void* McvBrowserView(void* mcv) {
  return Field<void*>(mcv, edge155::off::MCV_browser_view);
}
inline uint32_t McvOrientation(void* mcv) {
  return Field<uint32_t>(mcv, edge155::off::MCV_orientation);
}

// CalculateProposedLayout runs on the LayoutManagerBase subobject embedded at
// MCV_layout_base.
inline void* McvFromLayoutBase(void* layoutBase) {
  return reinterpret_cast<uint8_t*>(layoutBase) - edge155::off::MCV_layout_base;
}

// Build an EdgeMultiContentsResizeArea for `mcv` in `view` (EdgeResizeArea_size
// bytes from NewView); it starts hidden.
inline void ConstructResizeArea(void* view, void* mcv) {
  using CtorFn = void*(__fastcall*)(void*, void*);
  Callable<CtorFn>(edge155::rva::EdgeMultiContentsResizeArea_ctor)(view, mcv);
}

// Edge's own (single, two-pane) resize handle of a split.
inline void* McvResizeArea(void* mcv) {
  return Field<void*>(mcv, edge155::off::MCV_resize_area);
}

inline void* ContainerWebView(void* container) {
  return Field<void*>(container, edge155::off::CCV_contents_web_view);
}
inline void* ContainerWebContents(void* container) {
  return Field<void*>(ContainerWebView(container),
                      edge155::off::CWV_web_contents);
}

inline void* McvTabStripModel(void* mcv) {
  void* browserView = mcv ? McvBrowserView(mcv) : nullptr;
  return browserView ? TabStripModelOf(BrowserOf(browserView)) : nullptr;
}

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
  const bool isActiveTabInSplit =
      (activeTab != nullptr) && TabIsSplit(activeTab);
  if (!isActiveTabInSplit) {
    return 1;
  }
  OptionalSplitId optionalSplitId = TabSplitId(activeTab);
  if (!optionalSplitId.hasValue) {
    return 1;
  }
  const int memberCount = SplitMembers(tsm, optionalSplitId.token).Count();
  return memberCount < 1 ? 1 : memberCount;
}

// Run Edge's own split-view update for `browserView` as if `webContents` just
// changed: when that tab is no longer in a split, Edge collapses the view to
// one full-window pane (CloseSplitView). Used after a split is dissolved.
inline void MaybeUpdateSplitView(void* browserView, void* webContents) {
  using Fn = void(__fastcall*)(void*, void*);
  Callable<Fn>(edge155::rva::BrowserView_MaybeUpdateSplitView)(browserView,
                                                               webContents);
}

}  // namespace edgeplus::edge
