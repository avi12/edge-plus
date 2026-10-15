// state.h — per-MultiContentsView split tree registry. One concern: owning the
// live tree for each split so directional-add and drag can reshape it between
// layout passes. Until the UI mutates a tree, the geometry synthesizes a flat
// equal split from the live pane count, preserving the proven behaviour.
#pragma once
#include <map>
#include "pane_tree.h"

namespace fixedge {

inline std::map<void*, Node>& Trees() {
  static std::map<void*, Node> trees;
  return trees;
}

// The MultiContentsView of the split most recently laid out — the target for a
// directional-add command (the menu doesn't carry the MCV; the active split
// does). Set by the layout detour, read by the menu action.
inline void* g_activeMcv = nullptr;

// Whether the split feature is switched on. The DLL can be loaded for theming
// alone, so every user-facing split action checks this first; the panel pushes
// it (config.h). Defaults on: a DLL loaded before the panel's first config, or
// for the split alone, behaves as it always has.
inline bool g_splitActive = true;

inline bool SplitActive() {
  return g_splitActive;
}

// The tree for `mcv`, or nullptr if none has been set yet.
inline const Node* TreeFor(void* mcv) {
  const auto entry = Trees().find(mcv);
  return entry == Trees().end() ? nullptr : &entry->second;
}

inline void ForgetTree(void* mcv) {
  Trees().erase(mcv);
}

}  // namespace fixedge
