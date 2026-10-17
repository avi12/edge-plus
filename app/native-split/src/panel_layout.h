// panel_layout.h — where the Gemini panel's two pages sit. One concern, pure
// (no Edge, no Win32; unit-tested in test/panel_layout_test.cpp): the panel is
// docked at the right of the window's content, a slim strip of our own on top
// (the "share a tab" buttons) and Gemini under it. While the tab picker is open
// the strip grows to cover the whole panel.
#pragma once
#include "geom.h"

namespace fixedge::panel_layout {

// The docked panel's width in DIPs, and the strip's height when collapsed.
inline constexpr int kPanelWidth = 420;
inline constexpr int kStripHeight = 44;

struct Layout {
  Rect strip;
  Rect page;  // Gemini's page, under the strip
};

// The panel's width in a content area `contentWidth` wide: its own width, or
// half the area when that is narrower. The page keeps the rest.
inline int PanelWidth(int contentWidth) {
  const int half = contentWidth / 2;
  return kPanelWidth < half ? kPanelWidth : half;
}

// Lay the panel out in `content`, the area the page occupies. Both rects are in
// the same coordinates as `content`. Without a strip (Chrome's glic UI has
// none) Gemini's page fills the panel.
inline Layout PanelLayout(const Rect& content, bool isStripExpanded,
                          bool hasStrip = true) {
  const int width = PanelWidth(content.width);
  const int x = content.x + content.width - width;
  const int stripHeight = isStripExpanded || kStripHeight > content.height
                              ? content.height
                              : kStripHeight;
  Layout layout;
  if (!hasStrip) {
    layout.page = Rect{x, content.y, width, content.height};
    return layout;
  }
  layout.strip = Rect{x, content.y, width, stripHeight};
  layout.page =
      Rect{x, content.y + kStripHeight, width, content.height - kStripHeight};
  if (layout.page.height < 0) {
    layout.page.height = 0;
  }
  return layout;
}

}  // namespace fixedge::panel_layout
