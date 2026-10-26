// grip.h — the drag grip in each pane's hover bar, built with Edge's own
// AddButton so it looks and hovers like ⋯ / X. It has no click handler: views
// delivers its press, drags and release to views::Button's handlers, which
// hooks.h detours into drag.h. UI thread only.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include "edge/edge.h"
#include "chrome/pane/icons.h"
#include "split/split_data.h"

namespace edgeplus::grip {

struct Grip {
  void* button = nullptr;
  void* mcv = nullptr;
};

// Keyed by hover bar (ControlButtons view). std::less<> lets a bar be looked up
// by a pointer to const.
using GripMap = std::map<void*, Grip, std::less<>>;
inline GripMap& Grips() {
  static GripMap grips;
  return grips;
}

// The split view is being destroyed, and its panes' hover bars with it.
inline void ForgetSplitView(void* mcv) {
  std::erase_if(Grips(), [mcv](const auto& entry) {
    return entry.second.mcv == mcv;
  });
}

inline constexpr char kTooltip[] = "Drag to move this pane";

inline void EnsureGrips(void* mcv) {
  const int count = static_cast<int>(edge::McvCount(mcv));
  const bool isStockSplit = count <= split_data::kMinSplitMembers;
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
    edge::ViewReorderChild(bar, button, 0);  // grip, ⋯, X
    edge::ViewSetTooltip(button, kTooltip);
    edge::ControlButtonsRelayout(bar);
    Grips()[bar] = Grip{button, mcv};
    Log("[grip] pane %d got a grip", i);
  }
}

// Every bar of a grown split has a grip.
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

// The buttons are left unparented rather than deleted; their icon lives in
// Edge's heap.
inline void RemoveAll() {
  for (const auto& [bar, grip] : Grips()) {
    edge::ViewRemoveChild(bar, grip.button);
    edge::ControlButtonsRelayout(bar);
  }
  Grips().clear();
}

}  // namespace edgeplus::grip
