// active_tabs.h — which horizontal tab views have painted as the active one. One
// concern: a horizontal tab strip has no model-to-view lookup like the vertical
// list's (vtab_views.h), so the spine finds its tab among those seen painting as
// active, newest last. A remembered view may be freed or in another window, so
// it is only compared against a window's tree (vtab_views::ParentHolding), never
// read, until found there. UI thread only.
#pragma once
#include <cstddef>
#include <vector>
#include "vtab_views.h"

namespace fixedge::active_tabs {

inline constexpr size_t kRemembered = 8;
inline std::vector<void*> g_views;

inline void Note(void* view) {
  std::erase(g_views, view);
  g_views.push_back(view);
  if (g_views.size() > kRemembered) {
    g_views.erase(g_views.begin());
  }
}

// The newest remembered view that is still in `root`'s tree, or null.
inline void* NewestIn(void* root) {
  for (auto it = g_views.rbegin(); it != g_views.rend(); ++it) {
    if (vtab_views::ParentHolding(root, *it)) {
      return *it;
    }
  }
  return nullptr;
}

}  // namespace fixedge::active_tabs
