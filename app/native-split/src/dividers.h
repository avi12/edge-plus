// dividers.h — the draggable dividers between panes. One concern: keep one of
// Edge's own divider views (EdgeMultiContentsResizeArea, the 2-pane split's
// resize handle: same look, hover pill and resize cursor) per seam of a grown
// split's tree, place each over its seam on every layout pass, and turn a drag
// on one into a tree resize that keeps every other proportion. A split of N
// panes has N - 1 seams. Views are created outside layout and reused; surplus
// ones are hidden. UI thread only.
#pragma once
#include <map>
#include <vector>
#include "anim.h"
#include "edge.h"
#include "pane_tree.h"
#include "state.h"

namespace fixedge::dividers {

using namespace fixedge::edge155;

// The thickness Edge gives its own resize area (its preferred size).
inline constexpr int kThickness = 8;
// A stock split has two containers; dividers belong to grown splits only.
inline constexpr int kStockContainerCount = 2;

// Each grown split's divider views, and the seam each showed at the last layout.
struct Pool {
  std::vector<void*> views;
  std::vector<Seam> seams;
};
inline std::map<void*, Pool>& Pools() {
  static std::map<void*, Pool> pools;
  return pools;
}

// Make sure `mcv` has a divider view for every possible seam (one fewer than
// its containers) and that they sit above the panes in z-order.
inline void Ensure(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  if (count <= kStockContainerCount) {
    return;
  }
  Pool& pool = Pools()[mcv];
  const size_t needed = static_cast<size_t>(count - 1);
  while (pool.views.size() < needed) {
    void* view = edge::NewView(off::EdgeResizeArea_size);
    edge::ConstructResizeArea(view, mcv);  // starts hidden
    edge::AddChildViewAt(mcv, view, edge::McvChildInsertIndex(mcv));
    pool.views.push_back(view);
    Log("[divider] %zu views for %p", pool.views.size(), mcv);
  }
  for (void* view : pool.views) {
    edge::ViewReorderChild(mcv, view, edge::McvChildInsertIndex(mcv) - 1);
  }
}

// The divider's rect over `seam`, centred on the boundary.
inline Rect RectOf(const Seam& seam) {
  const int offset = seam.position - kThickness / 2;
  if (seam.axis == Axis::kHorizontal) {
    return Rect{offset, seam.crossStart, kThickness, seam.crossLength};
  }
  return Rect{seam.crossStart, offset, seam.crossLength, kThickness};
}

// Layout pass: one divider over each seam of `tree` in `area`, the rest (and
// Edge's own two-pane handle) hidden. Appends to the ChildLayouts vector.
inline void Place(void* mcv, const Node& tree, const Rect& area,
                  void* childLayouts) {
  auto entry = Pools().find(mcv);
  bool isShown = true;
  bool isHidden = false;
  Rect zeroRect{};
  if (void* edgeHandle = edge::McvResizeArea(mcv)) {
    edge::SetViewVisible(edgeHandle, false);
    edge::EmplaceChildLayout(childLayouts, &edgeHandle, &isHidden, &zeroRect);
  }
  if (entry == Pools().end()) {
    return;
  }
  Pool& pool = entry->second;
  pool.seams = Seams(tree, area);
  for (size_t i = 0; i < pool.views.size(); i++) {
    void* view = pool.views[i];
    const bool hasSeam = i < pool.seams.size();
    if (hasSeam) {
      Rect rect = RectOf(pool.seams[i]);
      edge::SetViewVisible(view, true);
      edge::EmplaceChildLayout(childLayouts, &view, &isShown, &rect);
    } else {
      edge::SetViewVisible(view, false);
      edge::EmplaceChildLayout(childLayouts, &view, &isHidden, &zeroRect);
    }
  }
}

// If `view` is one of our dividers, its split and the seam it shows.
inline bool Locate(const void* view, void** mcv, Seam* seam) {
  for (const auto& [split, pool] : Pools()) {
    for (size_t i = 0; i < pool.views.size(); i++) {
      const bool isThisDivider = pool.views[i] == view && i < pool.seams.size();
      if (isThisDivider) {
        *mcv = split;
        *seam = pool.seams[i];
        return true;
      }
    }
  }
  return false;
}

// --- dragging a divider ------------------------------------------------------

struct Drag {
  bool isActive = false;
  void* mcv = nullptr;
  Seam seam;        // measured at the press, so the result depends only on
  Node startTree;   // the total distance dragged
  Point pressedAt;  // window DIPs
};
inline Drag g_drag;

inline void Begin(void* mcv, const Seam& seam, Node startTree,
                  Point pressedAt) {
  g_drag = Drag{true, mcv, seam, std::move(startTree), pressedAt};
  anim::SetLive(mcv, true);  // the panes follow the pointer, no easing
  Log("[divider] drag started on a seam at %d (%s)", seam.position,
      seam.axis == Axis::kHorizontal ? "between columns" : "between rows");
}

inline void Move(Point rootLocation) {
  if (!g_drag.isActive) {
    return;
  }
  const bool isSideBySide = g_drag.seam.axis == Axis::kHorizontal;
  const int delta = isSideBySide ? rootLocation.x - g_drag.pressedAt.x
                                 : rootLocation.y - g_drag.pressedAt.y;
  Node tree = g_drag.startTree;
  if (!ResizeSeam(tree, g_drag.seam, delta)) {
    return;
  }
  Trees()[g_drag.mcv] = std::move(tree);
  edge::InvalidateLayout(g_drag.mcv);
  edge::LayoutImmediately(g_drag.mcv);
}

inline void End() {
  if (!g_drag.isActive) {
    return;
  }
  anim::SetLive(g_drag.mcv, false);
  g_drag = Drag{};
  Log("[divider] drag ended");
}

// Take every divider back out (before this DLL unloads). The views are left
// unparented rather than deleted, like the grips.
inline void RemoveAll() {
  End();
  for (auto& [mcv, pool] : Pools()) {
    for (void* view : pool.views) {
      edge::ViewRemoveChild(mcv, view);
    }
  }
  Pools().clear();
}

}  // namespace fixedge::dividers
