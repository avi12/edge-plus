// tab_views.h — asking every tab view to paint again, so a change of tab
// outline (tab_shape.h) shows at once. UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::tab_views {

inline constexpr int kMaxDepth = 64;

// A tab only asks for its outline when it paints, which may be a long way off.
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
