// panes.h — building and growing the pane containers. One concern: turn a
// 2-container MultiContentsView into an N-container one by replaying the ctor's
// own per-pane block (docs/re/ctor.md). UI thread only.
#pragma once
#include "edge.h"

namespace fixedge::panes {

// Allocate + construct one EdgeContentsContainerView, parent it under the MCV and
// append it to the container vector — exactly as the ctor builds panes 0 and 1.
// Returns false on a poisoned allocation.
inline bool AddContainer(void* mcv) {
  void* container = edge::NewView(0x558);  // sizeof(EdgeContentsContainerView)
  if (!container) {
    Log("[grow] alloc failed");
    return false;
  }
  edge::ConstructContainer(container, edge::McvBrowserView(mcv));
  if (edge::Field<uint8_t>(container, 0x179) == 1) {  // allocator poison flag
    Log("[grow] poisoned alloc");
    return false;
  }
  edge::AddChildViewAt(mcv, container, edge::McvChildInsertIndex(mcv));
  void* slot = container;  // raw_ptr temp for emplace_back
  edge::ContainersEmplaceBack(edge::McvContainerVector(mcv), &slot);
  edge::SetViewVisible(container, true);
  return true;
}

// Give pane `index` (>= 2) its hover bar (X / ⋯) if it has none, mirroring pane
// 1. Skipped when pane 0 has no bar either (the new toolbar draws no per-pane
// bars). Containers are reused across collapse/regrow, so this runs once each.
inline void EnsureControlButtons(void* mcv, int index) {
  if (index < 2 || index >= static_cast<int>(edge::McvCount(mcv))) return;
  void* container = edge::McvContainerAt(mcv, index);
  if (edge::ContainerControlButtons(container)) return;
  if (!edge::ContainerControlButtons(edge::McvContainerAt(mcv, 0))) return;
  edge::GiveControlButtons(container, edge::McvBrowserView(mcv), /*not_first=*/true);
  Log("[grow] pane %d got its control buttons", index);
}

// Grow to exactly `target` containers (no-op if already at/above). Returns the
// resulting count.
inline size_t GrowToPanes(void* mcv, int target) {
  size_t count = edge::McvCount(mcv);
  while (static_cast<int>(count) < target) {
    if (!AddContainer(mcv)) break;
    count = edge::McvCount(mcv);
  }
  return count;
}

}  // namespace fixedge::panes
