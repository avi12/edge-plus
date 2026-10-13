// tabs.h — wiring real web content into a pane. One concern: attach a tab's
// WebContents to a container's ContentsWebView (docs/re/tab-wiring.md). The
// directional-add flow will later hand a freshly opened tab here; for now the
// growth path feeds background tabs so grown panes aren't blank. UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::tabs {

// Attach the WebContents of strip tab `tab_index` to `container`'s pane. Returns
// false (no-op) if the index is out of range or there is no such tab. The caller
// must pass a genuine *background* tab index — never the active one or a tab
// already shown in another pane, or its content would be stolen from there.
inline bool FillPane(void* browser_view, void* container, int tab_index) {
  void* tsm = edge::TabStripModelOf(edge::BrowserOf(browser_view));
  if (!tsm) {
    Log("[feed] no TabStripModel");
    return false;
  }
  const int count = edge::TabCount(tsm);
  if (tab_index < 0 || tab_index >= count) {
    Log("[feed] tab %d out of range (count %d)", tab_index, count);
    return false;
  }
  void* web_contents = edge::TabWebContentsAt(tsm, tab_index);
  void* cwv = edge::ContainerWebView(container);
  Log("[feed] tab %d/%d wc=%p container=%p cwv=%p", tab_index, count, web_contents, container, cwv);
  if (!web_contents || !cwv) return false;
  edge::ContentsWebViewSetContents(cwv, web_contents);
  edge::WebContentsWasShown(web_contents);  // a background tab is hidden; show it so the pane paints
  return true;
}

}  // namespace fixedge::tabs
