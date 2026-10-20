// vtab_views.h — finding tab rows in the vertical tab list. One concern: walk the
// browser's view tree (read-only) for the tab row view at a model index. UI
// thread only.
#pragma once
#include <windows.h>
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

inline void Walk(void* view, int depth, Search* search) {
  const bool isDone = search->found || depth > kMaxDepth;
  if (isDone) {
    return;
  }
  const bool isTab = edge::VtableRva(view) == edge155::rva::VerticalTab_vftable;
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
