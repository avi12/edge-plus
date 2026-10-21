// panes.h — building and growing the pane containers. One concern: turn a
// 2-container MultiContentsView into an N-container one by replaying the ctor's
// own per-pane block (docs/re/ctor.md). UI thread only.
#pragma once
#include <utility>
#include "edge.h"

namespace fixedge::panes {

// Edge's own ctor builds panes 0 and 1; every pane from this index on is ours.
inline constexpr int kFirstGrownPane = 2;

// Allocate + construct one EdgeContentsContainerView, parent it under the MCV and
// append it to the container vector — exactly as the ctor builds panes 0 and 1.
// Returns false on a poisoned allocation.
inline bool AddContainer(void* mcv) {
  void* container = edge::NewView(edge155::off::EdgeContentsContainerView_size);
  if (!container) {
    Log("[grow] alloc failed");
    return false;
  }
  edge::ConstructContainer(container, edge::McvBrowserView(mcv));
  const bool isPoisoned =
      edge::Field<uint8_t>(container,
                           edge155::off::CONTAINER_allocation_poisoned) == 1;
  if (isPoisoned) {
    Log("[grow] poisoned alloc");
    return false;
  }
  edge::AddChildViewAt(mcv, container, edge::ViewChildCount(mcv));
  void* slot = container;  // raw_ptr temp for emplace_back
  edge::ContainersEmplaceBack(edge::McvContainerVector(mcv), &slot);
  edge::SetViewVisible(container, true);
  return true;
}

// Give pane `index` (a grown one) its hover bar (X / ⋯) if it has none,
// mirroring pane 1. Skipped when pane 0 has no bar either (the new toolbar draws
// no per-pane bars). Containers are reused across collapse/regrow, so this runs
// once each.
inline void EnsureControlButtons(void* mcv, int index) {
  const bool isGrownPane =
      index >= kFirstGrownPane && std::cmp_less(index, edge::McvCount(mcv));
  if (!isGrownPane) {
    return;
  }
  void* container = edge::McvContainerAt(mcv, index);
  const bool hasControlButtons =
      edge::ContainerControlButtons(container) != nullptr;
  if (hasControlButtons) {
    return;
  }
  const bool hasFirstPaneButtons =
      edge::ContainerControlButtons(edge::McvContainerAt(mcv, 0)) != nullptr;
  if (!hasFirstPaneButtons) {
    return;
  }
  // Pane 1's bar is built as a non-first pane's; a grown pane follows it.
  const bool isLaterPane = true;
  edge::GiveControlButtons(container, edge::McvBrowserView(mcv), isLaterPane);
  Log("[grow] pane %d got its control buttons", index);
}

}  // namespace fixedge::panes
