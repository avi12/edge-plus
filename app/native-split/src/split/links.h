// links.h — where a split pane's new-tab links (target=_blank, no opener) open:
// Edge's per-split ⋯ toggle "Open left links to the right" now means off: the
// pane itself goes there; on: a new pane to its right. Edge's two-pane handling
// of such links is replaced. UI thread. docs/re/split-links.md.
#pragma once
#include <utility>
#include "core/channel.h"
#include "edge/edge.h"
#include "split/split_data.h"
#include "split/split_ops.h"
#include "layout/state.h"

namespace edgeplus::links {

inline constexpr char kToggleLabel[] = "Open new-tab links in a new pane";

enum class LinkTarget : uint8_t {
  kEdge,  // not a split pane of this window
  kSamePane,
  kNewPaneRight,
};

inline bool IsForegroundTab(int disposition) {
  return disposition == edge155::off::DISPOSITION_new_foreground_tab ||
         disposition == edge155::off::DISPOSITION_split_foreground_tab;
}

// A page that opens a window to script it (an opener) keeps Edge's handling.
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

// Waits to become a pane once Edge has added the tab (channel kLinkPane).
struct NewPaneLink {
  void* source = nullptr;
  void* newContents = nullptr;
};
inline constinit NewPaneLink g_newPaneLink;

// --- the detours -------------------------------------------------------------

// BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation: asked before any
// window is made; Edge hands the link to its two-pane SplitWindowController.
// With the toggle on the request is left to make its tab, which AddNewContents
// below turns into a pane.
using ShouldOverride_t = bool(__fastcall*)(void*, void*, void*, int, bool,
                                           void*);
inline constinit ShouldOverride_t o_should_override = nullptr;
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

// BrowserWebContentsDelegate::AddNewContents: a background tab also lands right
// after the split.
using AddNewContents_t = void*(__fastcall*)(void*, void*, void**, const void*,
                                            int, const void*, bool, bool*);
inline constinit AddNewContents_t o_add_new_contents = nullptr;
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
inline constinit LinkThrottle_t o_link_throttle = nullptr;
inline void** __fastcall h_link_throttle(void** outThrottle,
                                         void* /*navigation*/) {
  *outThrottle = nullptr;
  return outThrottle;
}

// SplitWindowControllerImplV2::UpdateLinkedModeState: skipped; the toggle is
// read from the split when a link is clicked.
using UpdateLinkedMode_t = void(__fastcall*)(void*, bool);
inline constinit UpdateLinkedMode_t o_update_linked_mode = nullptr;
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

}  // namespace edgeplus::links
