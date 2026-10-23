// tab_views.h — the tab views of a browser window. One concern: walk the view
// tree (read-only) to ask every tab to paint again, so a change of tab outline
// (tab_shape.h) shows at once. UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::tab_views {

inline constexpr int kMaxDepth = 64;

// Queue a repaint of every tab under `view` (a row of the vertical list or a tab
// of the horizontal strip), and of the split row that holds it: a tab only asks
// for its outline when it paints, so without this the change shows at the next
// paint, which may be a long way off.
inline void RepaintTabs(void* view, int depth = 0) {
  if (depth > kMaxDepth) {
    return;
  }
  if (edge::IsEdgeTabView(view)) {
    edge::SchedulePaint(view);
    void* row = edge::ViewParent(view);
    const bool isSplitRow =
        row && edge::VtableRva(row) == edge155::rva::SplitTabView_vftable;
    if (isSplitRow) {
      edge::SchedulePaint(row);
    }
    return;
  }
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    RepaintTabs(edge::ViewChildAt(view, i), depth + 1);
  }
}

}  // namespace fixedge::tab_views
