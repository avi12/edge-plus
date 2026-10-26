// dividers.h — the draggable dividers between panes: one of Edge's own 2-pane
// resize handles (EdgeMultiContentsResizeArea) per seam of a grown split's
// tree. Views are created outside layout and reused. UI thread only.
#pragma once
#include <map>
#include <vector>
#include "layout/anim.h"
#include "edge/edge.h"
#include "layout/pane_tree.h"
#include "split/split_data.h"
#include "layout/state.h"

namespace edgeplus::dividers {

inline constexpr int kStockContainerCount = split_data::kMinSplitMembers;

struct Pool {
  std::vector<void*> views;
  std::vector<Seam> seams;  // as of the last layout
};
inline std::map<void*, Pool>& Pools() {
  static std::map<void*, Pool> pools;
  return pools;
}

// Above the panes in z-order.
inline void Ensure(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  if (count <= kStockContainerCount) {
    return;
  }
  Pool& pool = Pools()[mcv];
  const size_t needed = static_cast<size_t>(count - 1);
  while (pool.views.size() < needed) {
    void* view = edge::NewView(edge155::off::EdgeResizeArea_size);
    edge::ConstructResizeArea(view, mcv);
    edge::AddChildViewAt(mcv, view, edge::ViewChildCount(mcv));
    pool.views.push_back(view);
    Log("[divider] %zu views for %p", pool.views.size(), mcv);
  }
  for (void* view : pool.views) {
    edge::ViewReorderChild(mcv, view, edge::ViewChildCount(mcv) - 1);
  }
}

inline Rect RectOf(const Seam& seam) {
  if (seam.axis == Axis::kHorizontal) {
    return Rect{seam.position, seam.crossStart, seam.thickness,
                seam.crossLength};
  }
  return Rect{seam.crossStart, seam.position, seam.crossLength, seam.thickness};
}

inline void Place(void* mcv, const Node& tree, const Rect& area,
                  void* childLayouts) {
  auto entry = Pools().find(mcv);
  bool isShown = true;
  bool isHidden = false;
  Rect zeroRect{};
  // Edge's own two-pane handle stays VISIBLE but zero-sized (it cannot be hit):
  // UpdateContentsBorderAndOverlay reads its visibility as "this is a split",
  // and a hidden handle turns every pane's hover bar and border off.
  if (void* edgeHandle = edge::McvResizeArea(mcv)) {
    edge::SetViewVisible(edgeHandle, true);
    edge::EmplaceChildLayout(childLayouts, &edgeHandle, &isShown, &zeroRect);
  }
  if (entry == Pools().end()) {
    return;
  }
  Pool& pool = entry->second;
  pool.seams = Seams(tree, area, kPaneGap);
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

struct Drag {
  bool isActive = false;
  void* mcv = nullptr;
  // Measured at the press, so the result depends only on the total distance.
  Seam seam;
  Node startTree;
  Point pressedAt;  // window DIPs
};
inline constinit Drag g_drag;

inline void Begin(void* mcv, const Seam& seam, Node startTree,
                  Point pressedAt) {
  g_drag = Drag{true, mcv, seam, std::move(startTree), pressedAt};
  anim::SetLive(mcv, true);
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
  SetTree(g_drag.mcv, std::move(tree));
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

// Double-click, like Edge's own handle resets its two panes.
inline void ResetToEqual(void* mcv, const Seam& seam, Node tree) {
  if (!EqualizeSplit(tree, seam.path)) {
    return;
  }
  SetTree(mcv, std::move(tree));
  edge::InvalidateLayout(mcv);
  edge::LayoutImmediately(mcv);
  Log("[divider] reset a split to equal sizes");
}

// The split view is being destroyed, and its divider views with it.
inline void ForgetSplitView(void* mcv) {
  if (g_drag.mcv == mcv) {
    g_drag = Drag{};
  }
  Pools().erase(mcv);
}

// The views are left unparented rather than deleted, like the grips.
inline void RemoveAll() {
  End();
  for (auto& [mcv, pool] : Pools()) {
    for (void* view : pool.views) {
      edge::ViewRemoveChild(mcv, view);
    }
  }
  Pools().clear();
}

}  // namespace edgeplus::dividers
