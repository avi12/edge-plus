// offer.h — a new pane's tab offer. One concern: a pane added from a menu opens
// on Edge's split starting page, as Edge's own split button opens its second
// pane, so the user picks the pane's tab from the window's other tabs. Edge
// built the page for two panes: the picked tab replaces the split's LAST
// member and the page's X closes that member, because a two-pane split keeps
// its starting page there. In a grown split both act on the page's own pane,
// and a page stays an offer while its split is rebuilt. A two-pane split is
// Edge's own throughout. UI thread only.
// docs/re/split-offer.md.
#pragma once
#include <utility>
#include "channel.h"
#include "edge.h"
#include "split_data.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::offer {

using namespace fixedge::edge155;

// UpdateTabInSplit's flag as OpenTabInCurrentSplitWindow passes it: the picked
// tab takes the member's place and the member's tab closes.
inline constexpr int kReplaceMemberFlag = 0;

// Add a pane beside pane `besideLeaf`, in `direction`, showing the starting
// page in a new tab (closed again if the pane could not be added).
inline void AddOfferPane(void* mcv, Direction direction, int besideLeaf) {
  void* browserView = mcv ? edge::McvBrowserView(mcv) : nullptr;
  void* browser = browserView ? edge::BrowserOf(browserView) : nullptr;
  void* tsm = browser ? edge::TabStripModelOf(browser) : nullptr;
  if (!tsm) {
    return;
  }
  void* page = edge::AddBackgroundTab(browser, edge::StartingPageUrl(),
                                      edge::TabCount(tsm));
  void* pageTab = page ? edge::TabShowing(tsm, page) : nullptr;
  if (!pageTab) {
    Log("[offer] the starting page tab was not made");
    return;
  }
  const bool hasAdded = split_ops::AddPane(mcv, direction, besideLeaf, pageTab);
  if (!hasAdded) {
    edge::CloseTabAt(tsm, edge::IndexOfWebContents(tsm, page));
  }
}

// Whether `tab` is a member of a split of three or more.
inline bool IsInGrownSplit(void* tsm, void* tab) {
  return split_data::MemberCountOf(tsm, tab) > split_data::kMinSplitMembers;
}

// --- the pick -----------------------------------------------------------------

// SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow: the starting
// page's pick, for the active split. In a grown split the picked tab replaces
// the active member, the starting page the user picked in.
using OpenTab_t = void(__fastcall*)(void*, void*);
inline OpenTab_t o_open_tab = nullptr;
inline void __fastcall h_open_tab(void* controller, void* chosenContents) {
  void* tsm =
      edge::Field<void*>(controller, off::SPLITCONTROLLER_tab_strip_model);
  void* active = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool isGrownSplitOffer =
      active && IsInGrownSplit(tsm, active) &&
      edge::IsStartingPage(edge::TabContents(active));
  if (!isGrownSplitOffer) {
    o_open_tab(controller, chosenContents);
    return;
  }
  const int chosenIndex = edge::IndexOfWebContents(tsm, chosenContents);
  void* chosenTab = edge::TabModelAt(tsm, chosenIndex);
  const bool isChosenFree = split_data::IsFreeTab(chosenTab, active);
  if (!isChosenFree) {
    Log("[offer] picked tab %d is not a free tab", chosenIndex);
    return;
  }
  Log("[offer] picked tab %d replaces the starting page", chosenIndex);
  split_data::SafeUpdateTabInSplit(tsm, active, chosenIndex,
                                   kReplaceMemberFlag);
}

// --- the page leaving its split -------------------------------------------------

// The task Edge posts when a split whose last member is a starting page is
// removed (SplitWindowControllerImplV2::OnSplitTabRemoved): it turns the page
// into a new tab page. Growing a split or closing one of its panes removes the
// split and re-adds its tabs at once, so a page back in a split by the time the
// task runs stays the tab offer. The task then only drops its reference.
using PageLeftSplit_t = void(__fastcall*)(void*);
inline PageLeftSplit_t o_page_left_split = nullptr;
inline void __fastcall h_page_left_split(void* bindState) {
  void* page = edge::BoundWeakTarget(bindState);
  void* pageTab =
      page ? edge::TabShowing(edge::TsmForWindow(channel::g_window), page)
           : nullptr;
  const bool isBackInSplit = pageTab && edge::TabIsSplit(pageTab);
  if (isBackInSplit) {
    edge::ClearBoundWeakTarget(bindState);
  }
  o_page_left_split(bindState);
}

// --- the X ----------------------------------------------------------------------

// A grown split's starting page whose X was clicked, waiting for its pane to
// close (channel kCloseOfferPane) once the page's own call has returned.
inline void* g_closingPage = nullptr;

// SplitWindowStartingUIHandler::CloseSplitWindow: the starting page's X. In a
// grown split it closes the page's own pane.
using ClosePage_t = void(__fastcall*)(void*);
inline ClosePage_t o_close_page = nullptr;
inline void __fastcall h_close_page(void* handler) {
  void* tsm = edge::TsmForWindow(channel::g_window);
  void* page = edge::StartingPageOf(handler);
  void* pageTab = page ? edge::TabShowing(tsm, page) : nullptr;
  const bool isGrownSplitPage = pageTab && IsInGrownSplit(tsm, pageTab);
  if (!isGrownSplitPage) {
    o_close_page(handler);
    return;
  }
  g_closingPage = page;
  channel::Post(channel::CodeOf(channel::Command::kCloseOfferPane));
}

// Close the pane of the starting page whose X was clicked. Should its split
// have shrunk to two panes meanwhile, the page's tab closes, as Edge's own X
// would close it.
inline void CloseOfferPane() {
  void* page = std::exchange(g_closingPage, nullptr);
  void* mcv = edge::McvForWindow(channel::g_window);
  void* tsm = edge::TsmForWindow(channel::g_window);
  const int pane = mcv && page ? split_ops::LeafShowing(mcv, page) : kNoLeaf;
  const bool hasClosedPane = pane != kNoLeaf && split_ops::ClosePane(mcv, pane);
  if (hasClosedPane) {
    Log("[offer] starting page dismissed: pane %d closed", pane);
    return;
  }
  const int pageIndex = tsm && page ? edge::IndexOfWebContents(tsm, page) : -1;
  if (pageIndex >= 0) {
    edge::CloseTabAt(tsm, pageIndex);
  }
}

}  // namespace fixedge::offer
