// grip.h — the drag grip in each pane's hover bar. One concern: give every pane
// of a grown split a 6-dot grip button left of its ⋯ / X, built with Edge's own
// AddButton so it looks and hovers like them, and tell which pane a grip belongs
// to. The grip has no click handler: views delivers its press, drags and release
// to views::Button's handlers, which hooks.h detours into the drag (drag.h).
// UI thread only.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include "edge.h"
#include "icons.h"

namespace fixedge::grip {

// A grip button, and the split it belongs to.
struct Grip {
  void* button = nullptr;
  void* mcv = nullptr;
};

// Each hover bar (ControlButtons view) -> its grip. std::less<> lets a bar be
// looked up by a pointer to const.
using GripMap = std::map<void*, Grip, std::less<>>;
inline GripMap& Grips() {
  static GripMap grips;
  return grips;
}

inline constexpr char kTooltip[] = "Drag to move this pane";

// Give every pane bar of a grown split (> 2 containers) its grip, once.
inline void EnsureGrips(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  const bool isStockSplit = count <= 2;
  if (isStockSplit) {
    return;
  }
  for (int i = 0; i < count; i++) {
    void* bar = edge::ContainerControlButtons(edge::McvContainerAt(mcv, i));
    const bool shouldAddGrip = bar && !Grips().count(bar);
    if (!shouldAddGrip) {
      continue;
    }
    void* button = edge::ControlButtonsAddButton(
        bar, edge155::off::ControlButtons_more_string_id, icons::Grip(),
        /*clickable=*/false);
    edge::ViewReorderChild(bar, button, 0);  // first: grip, ⋯, X
    edge::ViewSetTooltip(button, kTooltip);
    edge::ControlButtonsRelayout(bar);  // widen the bar by one button
    Grips()[bar] = Grip{button, mcv};
    Log("[grip] pane %d got a grip", i);
  }
}

// If `bar` is the hover bar of a pane in a grown split (every such bar has a
// grip), that split and the pane's container index; else false.
inline bool LocateBar(const void* bar, void** mcv, int* pane) {
  const auto entry = Grips().find(bar);
  if (entry == Grips().end()) {
    return false;
  }
  void* split = entry->second.mcv;
  const int count = static_cast<int>(edge::McvCount(split));
  for (int i = 0; i < count; i++) {
    const bool isBarOfPane =
        edge::ContainerControlButtons(edge::McvContainerAt(split, i)) == bar;
    if (isBarOfPane) {
      *mcv = split;
      *pane = i;
      return true;
    }
  }
  return false;
}

// If `view` is a grip, the split it belongs to and the index of its pane's
// container (the pane being dragged); else false.
inline bool Locate(const void* view, void** mcv, int* pane) {
  for (const auto& [bar, grip] : Grips()) {
    const bool isThisGrip = grip.button == view;
    if (isThisGrip) {
      return LocateBar(bar, mcv, pane);
    }
  }
  return false;
}

inline bool IsGrip(const void* view) {
  void* mcv = nullptr;
  int pane = -1;
  return Locate(view, &mcv, &pane);
}

// Take every grip back out (before this DLL unloads). The buttons are left
// unparented rather than deleted; their icon lives in Edge's heap.
inline void RemoveAll() {
  for (const auto& [bar, grip] : Grips()) {
    edge::ViewRemoveChild(bar, grip.button);
    edge::ControlButtonsRelayout(bar);
  }
  Grips().clear();
}

}  // namespace fixedge::grip
