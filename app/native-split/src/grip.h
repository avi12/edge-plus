// grip.h — the drag grip in each pane's hover bar. One concern: give every pane
// of a grown split a 6-dot grip button left of its ⋯ / X, built with Edge's own
// AddButton so it looks and hovers like them, and tell which pane's grip lies
// under a point. The grip has no click handler; the press is caught by the window
// procedure (control.h), which starts the drag (drag.h). UI thread only.
#pragma once
#include <cstdint>
#include <map>
#include "edge.h"

namespace fixedge::grip {

using namespace fixedge::edge155;

// Edge's gfx::VectorIcon ABI: {reps, reps_size, name}, each rep {path, size},
// each path element a 4-byte command id or float argument.
struct IconRep {
  const void* path;
  size_t path_size;
};
struct VectorIcon {
  const IconRep* reps;
  size_t reps_size;
  const char* name;
};

// The 6-dot grip (two columns, three rows of r=1 dots on a 16 canvas), drawn
// like Edge's ⋯ icon. Built once in Edge's heap and never freed, so a grip that
// outlives this DLL (hot-unload) never points into unloaded memory.
inline const VectorIcon* Icon() {
  static const VectorIcon* icon = [] {
    constexpr int kDots = 6;
    constexpr size_t kPathSize = 2 + kDots * (3 + 7 + 1 + 7 + 1 + 1);  // canvas + per dot
    constexpr char kName[] = "kFixEdgePaneGripIcon";
    const size_t bytes = sizeof(VectorIcon) + sizeof(IconRep) + sizeof(kName) + kPathSize * 4;
    auto* block = static_cast<uint8_t*>(edge::EdgeNew(bytes));
    auto* out = reinterpret_cast<VectorIcon*>(block);
    auto* rep = reinterpret_cast<IconRep*>(block + sizeof(VectorIcon));
    auto* name = reinterpret_cast<char*>(block + sizeof(VectorIcon) + sizeof(IconRep));
    auto* path = reinterpret_cast<uint32_t*>(name + sizeof(kName));
    memcpy(name, kName, sizeof(kName));
    size_t n = 0;
    auto cmd = [&](int c) { path[n++] = static_cast<uint32_t>(c); };
    auto arg = [&](float f) { memcpy(&path[n++], &f, 4); };
    cmd(off::ICON_CANVAS_DIMENSIONS);
    arg(16.f);
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 2; ++col) {
        const float cx = 6.f + 4.f * static_cast<float>(col), cy = 4.f + 4.f * static_cast<float>(row);
        cmd(off::ICON_MOVE_TO);  // the dot's right-most point, then two half arcs
        arg(cx + 1.f);
        arg(cy);
        cmd(off::ICON_R_ARC_TO);
        for (float f : {1.f, 1.f, 0.f, 1.f, 1.f, -2.f, 0.f}) arg(f);
        cmd(off::ICON_R_ARC_TO);
        for (float f : {1.f, 1.f, 0.f, 1.f, 1.f, 2.f, 0.f}) arg(f);
        cmd(off::ICON_CLOSE);
      }
    }
    *rep = IconRep{path, n};
    *out = VectorIcon{rep, 1, name};
    return out;
  }();
  return icon;
}

// Each hover bar (ControlButtons view) -> its grip button.
inline std::map<void*, void*>& Grips() {
  static std::map<void*, void*> grips;
  return grips;
}

inline constexpr char kTooltip[] = "Drag to move this pane";

// Give every pane bar of a grown split (> 2 containers) its grip, once.
inline void EnsureGrips(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  if (count <= 2) return;
  for (int i = 0; i < count; ++i) {
    void* bar = edge::ContainerControlButtons(edge::McvContainerAt(mcv, i));
    if (!bar || Grips().count(bar)) continue;
    void* grip = edge::ControlButtonsAddButton(bar, off::ControlButtons_more_string_id, Icon(),
                                               /*clickable=*/false);
    edge::ViewReorderChild(bar, grip, 0);  // first: grip, ⋯, X
    edge::ViewSetTooltip(grip, kTooltip);
    edge::ControlButtonsRelayout(bar);  // widen the bar by one button
    Grips()[bar] = grip;
    Log("[grip] pane %d got a grip", i);
  }
}

// The pane whose grip is drawn under `root_point` (the window's client area in
// DIPs), else -1. Only drawn grips count, i.e. a hovered pane's bar.
inline int PaneAt(void* mcv, Point root_point) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  for (int i = 0; i < count; ++i) {
    void* bar = edge::ContainerControlButtons(edge::McvContainerAt(mcv, i));
    auto it = bar ? Grips().find(bar) : Grips().end();
    if (it == Grips().end() || !edge::ViewIsDrawn(it->second)) continue;
    Point p = root_point;
    edge::ConvertPoint(edge::ViewRoot(it->second), it->second, &p);
    const Rect b = edge::ViewBounds(it->second);
    if (p.x >= 0 && p.y >= 0 && p.x < b.w && p.y < b.h) return i;
  }
  return -1;
}

// Take every grip back out (before this DLL unloads). The buttons are left
// unparented rather than deleted; their icon lives in Edge's heap.
inline void RemoveAll() {
  for (auto& [bar, grip] : Grips()) {
    edge::ViewRemoveChild(bar, grip);
    edge::ControlButtonsRelayout(bar);
  }
  Grips().clear();
}

}  // namespace fixedge::grip
