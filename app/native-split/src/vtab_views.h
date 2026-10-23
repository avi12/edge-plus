// vtab_views.h — finding views in the browser's view tree. One concern: walk it
// (read-only) for the vertical tab row at a model index, the one nearest a
// point on screen, or the view that holds a given child. UI thread only.
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
  return edge::IsVerticalTabView(view);
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

// The horizontal strip's active tab view under `view`, or null (also when the
// strip has no tabs, as with the vertical list in use).
inline void* ActiveHorizontalTab(void* view, int depth = 0) {
  if (depth > kMaxDepth) {
    return nullptr;
  }
  if (edge::IsHorizontalTabView(view)) {
    const bool isActive = edge::IsTabObjectActive(
        static_cast<uint8_t*>(view) - edge155::off::EDGETAB_view_offset);
    return isActive ? view : nullptr;
  }
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    void* found = ActiveHorizontalTab(edge::ViewChildAt(view, i), depth + 1);
    if (found) {
      return found;
    }
  }
  return nullptr;
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

inline bool HoldsChild(void* view, void* child) {
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    if (edge::ViewChildAt(view, i) == child) {
      return true;
    }
  }
  return false;
}

// The view under `view` (itself included) that has `child` among its children,
// or null: `child` is only compared, never read, so it may already be freed
// (a parent that was destroyed took it along).
inline void* ParentHolding(void* view, void* child, int depth = 0) {
  if (depth > kMaxDepth) {
    return nullptr;
  }
  if (HoldsChild(view, child)) {
    return view;
  }
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    void* holder = ParentHolding(edge::ViewChildAt(view, i), child, depth + 1);
    if (holder) {
      return holder;
    }
  }
  return nullptr;
}

// Queue a repaint of every vertical tab row under `view`, and of the split row
// that holds it: a tab only asks for its outline when it paints, so a change of
// outline (tab_shape.h) shows at the next paint, which may be a long way off.
inline void RepaintTabRows(void* view, int depth = 0) {
  if (depth > kMaxDepth) {
    return;
  }
  if (IsTabRow(view)) {
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
    RepaintTabRows(edge::ViewChildAt(view, i), depth + 1);
  }
}

}  // namespace fixedge::vtab_views
