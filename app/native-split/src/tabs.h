// tabs.h — attaching a tab's WebContents to a pane's ContentsWebView
// (docs/re/tab-wiring.md). UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::tabs {

// `tabIndex` must be a background tab, never the active one or one shown in
// another pane, or its content would be stolen from there.
inline bool FillPane(void* browserView, void* container, int tabIndex) {
  void* tsm = edge::TabStripModelOf(edge::BrowserOf(browserView));
  if (!tsm) {
    Log("[feed] no TabStripModel");
    return false;
  }
  const int tabCount = edge::TabCount(tsm);
  const bool isInStrip = tabIndex >= 0 && tabIndex < tabCount;
  if (!isInStrip) {
    Log("[feed] tab %d out of range (count %d)", tabIndex, tabCount);
    return false;
  }
  void* webContents = edge::TabWebContentsAt(tsm, tabIndex);
  void* contentsWebView = edge::ContainerWebView(container);
  Log("[feed] tab %d/%d wc=%p container=%p cwv=%p", tabIndex, tabCount,
      webContents, container, contentsWebView);
  const bool canAttach = webContents && contentsWebView;
  if (!canAttach) {
    return false;
  }
  edge::ContentsWebViewSetContents(contentsWebView, webContents);
  // A background tab is hidden; show it so the pane paints.
  edge::WebContentsWasShown(webContents);
  return true;
}

}  // namespace fixedge::tabs
