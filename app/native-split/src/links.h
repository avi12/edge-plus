// links.h — where a split pane's new-tab links open. One concern: Edge's ⋯
// toggle "Open left links to the right" (one per split) now decides what a
// link that would open a new tab (target=_blank, no opener) does in a pane:
//   - off: the pane itself goes there; no tab is made;
//   - on: it opens in a new pane to the right of that pane.
// Edge's own split handling of such links (built for two panes) and its
// navigation throttle that sent the first pane's same-tab links to the last
// pane are replaced. Modifier clicks, popups and pages outside a split are
// Edge's own. UI thread only. docs/re/split-links.md.
#pragma once
#include <utility>
#include "channel.h"
#include "edge.h"
#include "split_data.h"
#include "split_ops.h"
#include "state.h"

namespace fixedge::links {

// The toggle's label, saying what it does now (Edge's says "Open left links to
// the right"); pane_menu.h puts it on the ⋯ menu.
inline constexpr char kToggleLabel[] = "Open new-tab links in a new pane";

// Where a page's new-tab link goes.
enum class LinkTarget : uint8_t {
  kEdge,          // not a split pane of this window: Edge's own handling
  kSamePane,      // the toggle is off
  kNewPaneRight,  // the toggle is on
};

// A link opening a foreground tab (Edge's split variant included).
inline bool IsForegroundTab(int disposition) {
  return disposition == edge155::off::DISPOSITION_new_foreground_tab ||
         disposition == edge155::off::DISPOSITION_split_foreground_tab;
}

// A plain new-tab link: clicked (a user gesture), for a foreground tab, and
// with no opener to keep (a page that opens a window to script it keeps
// Edge's handling).
inline bool IsNewTabLink(int disposition, bool hasUserGesture,
                         bool isOpenerSuppressed) {
  return IsForegroundTab(disposition) && hasUserGesture && isOpenerSuppressed;
}

inline LinkTarget LinkTargetFor(void* sourceContents) {
  void* tsm = edge::TsmForWindow(channel::g_window);
  void* tab = edge::TabShowing(tsm, sourceContents);
  const bool isSplitPane = tab && edge::TabIsSplit(tab);
  if (!isSplitPane) {
    return LinkTarget::kEdge;
  }
  return split_data::IsLinkModeOn(tsm, tab) ? LinkTarget::kNewPaneRight
                                            : LinkTarget::kSamePane;
}

// A link's new tab, waiting to become a pane once Edge has added it (channel
// kLinkPane).
struct NewPaneLink {
  void* source = nullptr;       // the pane's WebContents
  void* newContents = nullptr;  // the new tab's
};
inline NewPaneLink g_newPaneLink;

// --- the detours -------------------------------------------------------------

// BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation: Edge's own
// split hook, asked before any window is made. Edge hands a foreground-tab
// link from the split's active tab to its SplitWindowController, built for two
// panes. Instead: with the toggle off the pane goes to the link itself and the
// request is claimed (no window); with it on the request is left to make its
// tab, which AddNewContents below turns into a pane.
using ShouldOverride_t = bool(__fastcall*)(void*, void*, void*, int, bool,
                                           void*);
inline ShouldOverride_t o_should_override = nullptr;
inline bool __fastcall h_should_override(void* delegate, void* source,
                                         void* opener, int disposition,
                                         bool argument5, void* params) {
  const bool isNewTabLink =
      params && IsForegroundTab(disposition) &&
      edge::Field<bool>(params, edge155::off::NEWWINDOW_opener_suppressed);
  const LinkTarget target =
      isNewTabLink ? LinkTargetFor(source) : LinkTarget::kEdge;
  switch (target) {
    case LinkTarget::kSamePane:
      Log("[links] new-tab link: opened in its own pane");
      edge::NavigateTo(source, static_cast<uint8_t*>(params) +
                                   edge155::off::NEWWINDOW_target_url);
      return true;
    case LinkTarget::kNewPaneRight:
      return false;
    default:
      return o_should_override(delegate, source, opener, disposition, argument5,
                               params);
  }
}

// BrowserWebContentsDelegate::AddNewContents: with the toggle on, the link's
// tab goes in quietly (in the background, which also places it right after
// the split), and becomes a pane once Edge is done adding it.
using AddNewContents_t = void*(__fastcall*)(void*, void*, void**, const void*,
                                            int, const void*, bool, bool*);
inline AddNewContents_t o_add_new_contents = nullptr;
inline void* __fastcall h_add_new_contents(
    void* delegate, void* source, void** newContents, const void* targetUrl,
    int disposition, const void* windowFeatures, bool hasUserGesture,
    bool* wasBlocked) {
  void* contents = newContents ? *newContents : nullptr;
  const bool isNewTabLink =
      contents &&
      IsNewTabLink(disposition, hasUserGesture, !edge::HasOpener(contents));
  const bool isToNewPane =
      isNewTabLink && LinkTargetFor(source) == LinkTarget::kNewPaneRight;
  if (!isToNewPane) {
    return o_add_new_contents(delegate, source, newContents, targetUrl,
                              disposition, windowFeatures, hasUserGesture,
                              wasBlocked);
  }
  g_newPaneLink = NewPaneLink{source, contents};
  void* result =
      o_add_new_contents(delegate, source, newContents, targetUrl,
                         edge155::off::DISPOSITION_new_background_tab,
                         windowFeatures, hasUserGesture, wasBlocked);
  channel::Post(channel::CodeOf(channel::Command::kLinkPane));
  return result;
}

// SplitWindowLinkModeNavigationThrottle::MaybeCreateThrottleFor: never made, so
// Edge no longer sends the first pane's same-tab links to the last pane.
using LinkThrottle_t = void**(__fastcall*)(void**, void*);
inline LinkThrottle_t o_link_throttle = nullptr;
inline void** __fastcall h_link_throttle(void** outThrottle,
                                         void* /*navigation*/) {
  *outThrottle = nullptr;
  return outThrottle;
}

// SplitWindowControllerImplV2::UpdateLinkedModeState: the toggle now only
// means what this header says, read from the split when a link is clicked, so
// Edge's two-pane wiring of it is skipped.
using UpdateLinkedMode_t = void(__fastcall*)(void*, bool);
inline UpdateLinkedMode_t o_update_linked_mode = nullptr;
inline void __fastcall h_update_linked_mode(void* /*controller*/,
                                            bool /*isOn*/) {
}

// --- the new pane (channel kLinkPane, after AddNewContents returned) --------

inline void OpenLinkPane() {
  const NewPaneLink link = std::exchange(g_newPaneLink, NewPaneLink{});
  void* mcv = edge::McvForWindow(channel::g_window);
  void* tsm = edge::TsmForWindow(channel::g_window);
  void* newTab = edge::TabShowing(tsm, link.newContents);
  const int sourceLeaf =
      mcv ? split_ops::LeafShowing(mcv, link.source) : kNoLeaf;
  const bool canAddPane = newTab && sourceLeaf != kNoLeaf;
  if (!canAddPane) {
    Log("[links] new-tab link: its pane or tab went away");
    return;
  }
  Log("[links] new-tab link: a new pane right of pane %d", sourceLeaf);
  split_ops::AddPane(mcv, Direction::kRight, sourceLeaf, newTab);
}

}  // namespace fixedge::links
