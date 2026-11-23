// split_view_detours.h — the detours on Edge's split view (MultiContentsView)
// and the split bookkeeping around it, so a split grown past two panes is fed,
// laid out, focused, reloaded, measured and closed as Edge does two. Every
// detour is gated so a stock (<=2) split runs the original untouched.
#pragma once
#include <cstdint>
#include "chrome/pane/dividers.h"
#include "chrome/pane/drag.h"
#include "chrome/pane/grip.h"
#include "chrome/tabs/tabdrag.h"
#include "chrome/tabs/tabrows.h"
#include "core/log.h"
#include "edge/api/binding.h"
#include "edge/api/browser.h"
#include "edge/api/split_controls.h"
#include "edge/api/split_model.h"
#include "edge/api/split_view.h"
#include "edge/api/tab_strip.h"
#include "edge/api/views.h"
#include "edge/api/web_contents.h"
#include "edge/offsets.h"
#include "layout/anim.h"
#include "layout/geometry.h"
#include "layout/state.h"
#include "split/panes.h"
#include "split/split_data.h"

namespace edgeplus::hooks {

// MultiContentsView::SetWebContentsAtIndex — the feeder's one call. Its index<2
// guard is lifted at install (the safety patches in InstallAll).
using SetWc_t = void*(__fastcall*)(void*, void*, uint32_t);
inline constinit SetWc_t o_setwc = nullptr;
inline void* __fastcall h_setwc(void* mcv, void* webContents, uint32_t index) {
  const bool shouldBuildContainers = index >= edge::McvCount(mcv);
  if (shouldBuildContainers) {
    Log("[grow] feeder wants index %u; count %llu - building container(s)",
        index, static_cast<unsigned long long>(edge::McvCount(mcv)));
    while (edge::McvCount(mcv) <= index) {
      const bool isAdded = panes::AddContainer(mcv);
      if (!isAdded) {
        break;
      }
    }
  }
  void* result = o_setwc(mcv, webContents, index);
  // Edge only force-paints the panes it natively tracks, and only watches the
  // first and last member for hover; a fed background member stays blank until
  // shown.
  if (webContents) {
    edge::WebContentsWasShown(webContents);
    edge::ListenSplitInput(webContents);
  }
  panes::EnsureControlButtons(mcv, static_cast<int>(index));
  grip::EnsureGrips(mcv);
  dividers::Ensure(mcv);
  return result;
}

// LayoutManagerBase::CalculateProposedLayout — `self` is the subobject at
// MCV+0x340; `sizeBounds` is a const SizeBounds& (offsets.h SIZEBOUNDS_*).
using Calc_t = void*(__fastcall*)(void*, void*, void*);
inline constinit Calc_t o_calc = nullptr;
inline void* __fastcall h_calc(void* self, void* out, void* sizeBounds) {
  void* mcv = edge::McvFromLayoutBase(self);
  g_activeMcv = mcv;
  // A split torn off into another window leaves without CloseSplitView: the
  // old window's grown panes kept its pages, now shown in the new window. A
  // page under two windows' accessibility trees is the suspected cause of a
  // hang seen in Edge's AXPosition walks once the old window was focused.
  // With no split shown, no grown pane holds a page.
  const bool isNoSplitShown =
      edge::McvActiveSplitPanes(mcv) < split_data::kMinSplitMembers;
  if (isNoSplitShown) {
    panes::EmptyGrownContainers(mcv);
  }
  tabdrag::OnLaidOut(mcv);
  const bool isBounded =
      edge::Field<uint8_t>(sizeBounds, edge155::off::SIZEBOUNDS_has_width) ==
          1 &&
      edge::Field<uint8_t>(sizeBounds, edge155::off::SIZEBOUNDS_has_height) ==
          1;
  const int width =
      edge::Field<int32_t>(sizeBounds, edge155::off::SIZEBOUNDS_width);
  const int height =
      edge::Field<int32_t>(sizeBounds, edge155::off::SIZEBOUNDS_height);
  // On every pass, native too, so a resize snaps.
  const bool isResized = isBounded && anim::NoteHost(mcv, width, height);
  const bool isNativeLayout =
      !SplitActive() ||
      edge::McvCount(mcv) <=
          static_cast<size_t>(dividers::kStockContainerCount) ||
      !isBounded;
  if (isNativeLayout) {
    return o_calc(self, out, sizeBounds);
  }
  // 1 once collapsed; surplus containers are hidden.
  const int paneCount = edge::McvActiveSplitPanes(mcv);
  return geometry::TilePanes(mcv, out, width, height, paneCount, isResized);
}

// MultiContentsView::SetActiveIndex — Edge CHECKs index < 2 (int3 when a pane
// 3+ takes focus) and only clears the primary flag on "the other" of panes 0/1,
// so a grown split does the same work for N panes.
using SetActive_t = void(__fastcall*)(void*, int);
inline constinit SetActive_t o_set_active = nullptr;
inline void __fastcall h_set_active(void* mcv, int index) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  const bool isStockSplit = count <= dividers::kStockContainerCount;
  if (isStockSplit) {
    return o_set_active(mcv, index);
  }
  const bool isVisiblePane =
      index >= 0 && index < count &&
      edge::ViewGetVisible(edge::McvContainerAt(mcv, index));
  if (!isVisiblePane) {
    Log("[active] ignored SetActiveIndex(%d) of %d", index, count);
    return;
  }
  edge::Field<int32_t>(mcv, edge155::off::MCV_active_index) = index;
  for (int i = 0; i < count; i++) {
    edge::Field<uint8_t>(edge::ContainerWebView(edge::McvContainerAt(mcv, i)),
                         edge155::off::CWV_primary_flag) = i == index ? 1 : 0;
  }
  edge::McvUpdateBorders(mcv);
  Log("[active] pane %d of %d", index, count);
}

// MultiContentsView::~MultiContentsView: every pointer we keep to the view goes
// first. A flight still animating it relaid out the destroyed view on the next
// tick (seen: tearing a split off into its own window crashed Edge).
using McvDtor_t = void(__fastcall*)(void*);
inline constinit McvDtor_t o_mcv_dtor = nullptr;
inline void __fastcall h_mcv_dtor(void* mcv) {
  anim::ForgetSplitView(mcv);
  dividers::ForgetSplitView(mcv);
  grip::ForgetSplitView(mcv);
  drag::ForgetSplitView(mcv);
  tabdrag::ForgetSplitView(mcv);
  ForgetSplitView(mcv);
  Log("[mcv] a split view is going: forgot it");
  o_mcv_dtor(mcv);
}

// MultiContentsView::CloseSplitView: Edge empties pane 1 as the split closes;
// our panes are emptied with it. Every way a split shrinks or goes (a pane
// closed or dragged out, the split dissolved, another tab shown) comes through
// here, since Edge only ever removes a member by dissolving the split.
using CloseSplitView_t = void(__fastcall*)(void*);
inline constinit CloseSplitView_t o_close_split_view = nullptr;
inline void __fastcall h_close_split_view(void* mcv) {
  o_close_split_view(mcv);
  panes::EmptyGrownContainers(mcv);
}

// chrome::ReloadInternal(browser, disposition, bypassCache): reloads every
// selected tab once more are selected than the active tab's split needs, and a
// split of three or more has all its members selected, so the reload button
// reloaded every pane. In such a split the active pane alone is reloaded.
using ReloadInternal_t = void(__fastcall*)(void*, int32_t, bool);
inline constinit ReloadInternal_t o_reload_internal = nullptr;
inline void __fastcall h_reload_internal(void* browser, int32_t disposition,
                                         bool isBypassingCache) {
  void* const tsm = edge::TabStripModelOf(browser);
  void* const activeTab = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool isActiveTabInSplit =
      (activeTab != nullptr) && edge::TabIsSplit(activeTab);
  edge::OptionalSplitId splitId = {};
  int paneCount = 0;
  if (isActiveTabInSplit) {
    splitId = edge::TabSplitId(activeTab);
    paneCount =
        splitId.hasValue ? edge::SplitMembers(tsm, splitId.token).Count() : 0;
  }
  const bool isGrownSplit = paneCount > split_data::kMinSplitMembers;
  const bool isInCurrentTab =
      disposition == edge155::off::DISPOSITION_current_tab;
  const bool isReloadingOnlyActivePane = isGrownSplit && isInCurrentTab;
  if (!isReloadingOnlyActivePane) {
    return o_reload_internal(browser, disposition, isBypassingCache);
  }
  edge::ReloadPage(edge::TabContents(activeTab),
                   isBypassingCache ? edge155::off::RELOADTYPE_bypassing_cache
                                    : edge155::off::RELOADTYPE_normal);
  Log("[reload] active pane of a %d-pane split only", paneCount);
}

// MultiContentsView::GetFocusableViewsMapFor: Edge keeps a focus map only for
// the two containers it built, so for pane 3+ the lookup is null, and
// switching to a tab whose pane is ours crashed in the caller. Our panes get
// an empty map instead: no stored focus to restore there.
using FocusMap_t = void*(__fastcall*)(void*, void*);
inline constinit FocusMap_t o_focus_map = nullptr;
inline void* __fastcall h_focus_map(void* mcv, void* container) {
  void* focusMap = o_focus_map(mcv, container);
  if (focusMap) {
    return focusMap;
  }
  alignas(8) static uint8_t emptyMap[edge155::off::FocusableViewsMap_size] = {};
  return emptyMap;
}

// split_tabs::LogSplitView{Created,Updated}UKM: Edge's split metrics CHECK that
// a split has exactly two tabs, so a grown split crashed Edge on them (seen: a
// pane of three navigating).
using SplitUkm_t = void(__fastcall*)(void*, void*);
inline constinit SplitUkm_t o_split_created_ukm = nullptr;
inline constinit SplitUkm_t o_split_updated_ukm = nullptr;
inline bool IsTwoTabSplit(void* tsm, void* splitId) {
  return edge::SplitMembers(tsm, splitId).Count() == tabrows::kPairedMembers;
}
inline void __fastcall h_split_created_ukm(void* tsm, void* splitId) {
  if (IsTwoTabSplit(tsm, splitId)) {
    o_split_created_ukm(tsm, splitId);
  }
}
inline void __fastcall h_split_updated_ukm(void* tsm, void* splitId) {
  if (IsTwoTabSplit(tsm, splitId)) {
    o_split_updated_ukm(tsm, splitId);
  }
}

}  // namespace edgeplus::hooks
