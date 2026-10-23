// state.h — the live split tree of each MultiContentsView. Until the UI edits a
// tree, the geometry uses a flat equal split of the live pane count.
#pragma once
#include <map>
#include "pane_tree.h"

namespace fixedge {

inline std::map<void*, Node>& Trees() {
  static std::map<void*, Node> trees;
  return trees;
}

// The split most recently laid out: the target of a directional-add command,
// since the menu doesn't carry the MCV.
inline constinit void* g_activeMcv = nullptr;

// The DLL can be loaded for theming alone, so every user-facing split action
// checks this. On by default, for a DLL loaded before the panel's first config.
inline constinit bool g_splitActive = true;

inline bool SplitActive() {
  return g_splitActive;
}

inline const Node* TreeFor(void* mcv) {
  const auto entry = Trees().find(mcv);
  return entry == Trees().end() ? nullptr : &entry->second;
}

inline void ForgetTree(void* mcv) {
  Trees().erase(mcv);
}

}  // namespace fixedge
