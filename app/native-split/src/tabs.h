// tabs.h — wiring real web content into a pane. One concern: attach a tab's
// WebContents to a container's ContentsWebView (docs/re/tab-wiring.md). The
// directional-add flow will later hand a freshly opened tab here; for now the
// growth path feeds background tabs so grown panes aren't blank. UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::tabs {

// Attach the WebContents of strip tab `tabIndex` to `container`'s pane. Returns
// false (no-op) if the index is out of range or there is no such tab. The caller
// must pass a genuine *background* tab index — never the active one or a tab
// already shown in another pane, or its content would be stolen from there.
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
