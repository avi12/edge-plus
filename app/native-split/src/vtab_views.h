// vtab_views.h — finding tab rows in the vertical tab list. One concern: walk the
// browser's view tree (read-only) for the tab row view at a model index, or
// the one nearest a point on screen. UI
// thread only.
#pragma once
#include <windows.h>
#include <climits>
#include <cstdlib>
#include "edge.h"

namespace fixedge::vtab_views {

inline constexpr int kMaxDepth = 64;

// What a walk looks for: the tab at a model index. Tab rows are numbered in tree
// order.
struct Search {
  int wantedIndex = -1;
  int seen = 0;
  void* found = nullptr;
};

inline bool IsTabRow(void* view) {
  return edge::VtableRva(view) == edge155::rva::VerticalTab_vftable;
}

inline void Walk(void* view, int depth, Search* search) {
  const bool isDone = search->found || depth > kMaxDepth;
  if (isDone) {
    return;
  }
  if (IsTabRow(view)) {
    if (search->seen == search->wantedIndex) {
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

// What a walk looks for: the tab row whose top is nearest a screen y (DIPs).
struct NearestSearch {
  int screenY = 0;
  int bestDistance = INT_MAX;
  void* found = nullptr;
};

inline void WalkNearest(void* view, int depth, NearestSearch* search) {
  if (depth > kMaxDepth) {
    return;
  }
  if (IsTabRow(view)) {
    const int distance =
        std::abs(edge::ViewBoundsInScreen(view).y - search->screenY);
    if (distance < search->bestDistance) {
      search->bestDistance = distance;
      search->found = view;
    }
    return;
  }
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    WalkNearest(edge::ViewChildAt(view, i), depth + 1, search);
  }
}

// The tab row whose top is nearest `screenY` (screen DIPs), or null.
inline void* TabRowNearest(void* anyView, int screenY) {
  NearestSearch search;
  search.screenY = screenY;
  WalkNearest(edge::ViewRoot(anyView), 0, &search);
  return search.found;
}

}  // namespace fixedge::vtab_views
