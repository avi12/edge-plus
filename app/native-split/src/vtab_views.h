// vtab_views.h — finding tab rows in the vertical tab list. One concern: walk the
// browser's view tree (read-only) for the tab row views: all of them, or the one
// at a model index. UI thread only.
#pragma once
#include <windows.h>
#include "edge.h"

namespace fixedge::vtab_views {

using namespace fixedge::edge155;

inline constexpr int kMaxDepth = 64;

// What a walk looks for: the tab at a model index. Tab rows are numbered in tree
// order.
struct Search {
  int wantedIndex = -1;
  int seen = 0;
  void* found = nullptr;
};

inline void Walk(void* view, int depth, Search* search) {
  const bool isDone = search->found || depth > kMaxDepth;
  if (isDone) {
    return;
  }
  const bool isTab = edge::VtableRva(view) == rva::VerticalTab_vftable;
  if (isTab) {
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

// Every tab row, in tree order, up to `capacity` of them.
inline void Collect(void* view, int depth, void** out, size_t capacity,
                    size_t* count) {
  const bool isDone = *count >= capacity || depth > kMaxDepth;
  if (isDone) {
    return;
  }
  const bool isTab = edge::VtableRva(view) == rva::VerticalTab_vftable;
  if (isTab) {
    out[(*count)++] = view;
    return;
  }
  const size_t childCount = edge::ViewChildCount(view);
  for (size_t i = 0; i < childCount; i++) {
    Collect(edge::ViewChildAt(view, i), depth + 1, out, capacity, count);
  }
}

inline size_t AllTabs(void* anyView, void** out, size_t capacity) {
  size_t count = 0;
  Collect(edge::ViewRoot(anyView), 0, out, capacity, &count);
  return count;
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

}  // namespace fixedge::vtab_views
