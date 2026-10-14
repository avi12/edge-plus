// grip.h — the drag grip in each pane's hover bar. One concern: give every pane
// of a grown split a 6-dot grip button left of its ⋯ / X, built with Edge's own
// AddButton so it looks and hovers like them, and tell which pane a grip belongs
// to. The grip has no click handler: views delivers its press, drags and release
// to views::Button's handlers, which hooks.h detours into the drag (drag.h).
// UI thread only.
#pragma once
#include <cstdint>
#include <map>
#include "edge.h"

namespace fixedge::grip {

using namespace fixedge::edge155;

// Edge's gfx::VectorIcon ABI — Chromium's VectorIcon {reps, reps_size, name},
// each VectorIconRep {path, path_size}, each path element a 4-byte command id or
// float argument.
struct IconRepresentation {
  const void* path;
  size_t pathSize;
};
struct VectorIcon {
  const IconRepresentation* representations;
  size_t representationCount;
  const char* name;
};

// The 6-dot grip (two columns, three rows of r=1 dots on a 16 canvas), drawn
// like Edge's ⋯ icon. Built once in Edge's heap and never freed, so a grip that
// outlives this DLL (hot-unload) never points into unloaded memory.
inline const VectorIcon* Icon() {
  static const VectorIcon* icon = [] {
    constexpr int kRows = 3;
    constexpr int kColumns = 2;
    constexpr int kDotCount = kRows * kColumns;
    // CANVAS_DIMENSIONS + its size, then per dot: MOVE_TO + x y, two R_ARC_TO
    // + 7 arguments each, CLOSE.
    constexpr size_t kPathSize = 2 + kDotCount * (3 + 7 + 1 + 7 + 1 + 1);
    constexpr char kName[] = "kFixEdgePaneGripIcon";
    constexpr float kCanvasSize = 16.f;
    constexpr float kFirstCenterX = 6.f;
    constexpr float kFirstCenterY = 4.f;
    constexpr float kDotSpacing = 4.f;
    constexpr float kDotRadius = 1.f;
    const size_t byteCount = sizeof(VectorIcon) + sizeof(IconRepresentation) +
                             sizeof(kName) + kPathSize * sizeof(uint32_t);
    auto* block = static_cast<uint8_t*>(edge::EdgeNew(byteCount));
    auto* vectorIcon = reinterpret_cast<VectorIcon*>(block);
    auto* representation =
        reinterpret_cast<IconRepresentation*>(block + sizeof(VectorIcon));
    auto* name = reinterpret_cast<char*>(block + sizeof(VectorIcon) +
                                         sizeof(IconRepresentation));
    auto* path = reinterpret_cast<uint32_t*>(name + sizeof(kName));
    memcpy(name, kName, sizeof(kName));
    size_t elementCount = 0;
    auto appendCommand = [&](int command) {
      path[elementCount] = static_cast<uint32_t>(command);
      elementCount++;
    };
    auto appendArgument = [&](float argument) {
      memcpy(&path[elementCount], &argument, sizeof(argument));
      elementCount++;
    };
    appendCommand(off::ICON_CANVAS_DIMENSIONS);
    appendArgument(kCanvasSize);
    for (int row = 0; row < kRows; row++) {
      for (int column = 0; column < kColumns; column++) {
        const float centerX =
            kFirstCenterX + kDotSpacing * static_cast<float>(column);
        const float centerY =
            kFirstCenterY + kDotSpacing * static_cast<float>(row);
        // The dot's right-most point, then two half arcs (rx ry rotation
        // large-arc sweep dx dy) around it.
        appendCommand(off::ICON_MOVE_TO);
        appendArgument(centerX + kDotRadius);
        appendArgument(centerY);
        appendCommand(off::ICON_R_ARC_TO);
        for (const float argument : {1.f, 1.f, 0.f, 1.f, 1.f, -2.f, 0.f}) {
          appendArgument(argument);
        }
        appendCommand(off::ICON_R_ARC_TO);
        for (const float argument : {1.f, 1.f, 0.f, 1.f, 1.f, 2.f, 0.f}) {
          appendArgument(argument);
        }
        appendCommand(off::ICON_CLOSE);
      }
    }
    *representation = IconRepresentation{path, elementCount};
    *vectorIcon = VectorIcon{representation, 1, name};
    return vectorIcon;
  }();
  return icon;
}

// A grip button, and the split it belongs to.
struct Grip {
  void* button = nullptr;
  void* mcv = nullptr;
};

// Each hover bar (ControlButtons view) -> its grip.
inline std::map<void*, Grip>& Grips() {
  static std::map<void*, Grip> grips;
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
        bar, off::ControlButtons_more_string_id, Icon(),
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
  const auto entry = Grips().find(const_cast<void*>(bar));
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
