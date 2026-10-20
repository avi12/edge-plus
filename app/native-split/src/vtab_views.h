// vtab_views.h — finding tab rows in the vertical tab list. One concern: walk the
// browser's view tree (read-only) for the tab row views, by position in the
// model's order or by the screen point they cover. UI thread only.
#pragma once
#include <windows.h>
#include "edge.h"

namespace fixedge::vtab_views {

using namespace fixedge::edge155;

inline constexpr int kMaxDepth = 64;

// What a walk looks for: the tab at a model index, or the tab covering a point
// (screen pixels, `scale` px per DIP). Tab rows are numbered in tree order.
struct Search {
  int wantedIndex = -1;
  bool isLookingForPoint = false;
  POINT point{};
  float scale = 1.f;
  int seen = 0;
  void* found = nullptr;
};

inline bool Covers(void* tab, const Search& search) {
  const Rect bounds = edge::ViewBoundsInScreen(tab);
  const float left = static_cast<float>(bounds.x) * search.scale;
  const float top = static_cast<float>(bounds.y) * search.scale;
  const float right = left + static_cast<float>(bounds.width) * search.scale;
  const float bottom = top + static_cast<float>(bounds.height) * search.scale;
  return static_cast<float>(search.point.x) >= left &&
         static_cast<float>(search.point.x) < right &&
         static_cast<float>(search.point.y) >= top &&
         static_cast<float>(search.point.y) < bottom;
}

inline void Walk(void* view, int depth, Search* search) {
  const bool isDone = search->found || depth > kMaxDepth;
  if (isDone) {
    return;
  }
  const bool isTab = edge::VtableRva(view) == rva::VerticalTab_vftable;
  if (isTab) {
    const bool isWanted = search->isLookingForPoint
                              ? Covers(view, *search)
                              : search->seen == search->wantedIndex;
    if (isWanted) {
      search->found = view;
    }
    search->seen++;
    return;
  }
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    Walk(edge::ViewChildAt(view, i), depth + 1, search);
  }
}

// The selected tab's row view, or null when it can't be told.
inline void* SelectedTab(void* tsm, void* anyView) {
  void* activeTab = edge::ActiveTab(tsm);
  const int index = activeTab ? edge::GetIndexOfTabRecursive(
                                    edge::RootCollection(tsm), activeTab)
                              : -1;
  if (index < 0) {
    return nullptr;
  }
  Search search;
  search.wantedIndex = index;
  Walk(edge::ViewRoot(anyView), 0, &search);
  return search.found;
}

// The tab row under the screen point `point`, or null.
inline void* TabAtPoint(void* anyView, POINT point, float scale) {
  Search search;
  search.isLookingForPoint = true;
  search.point = point;
  search.scale = scale;
  Walk(edge::ViewRoot(anyView), 0, &search);
  return search.found;
}

}  // namespace fixedge::vtab_views
