// state.h — the live split tree of each split. Until the UI edits a tree, the
// geometry uses a flat equal split of the live pane count.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include "edge.h"
#include "pane_tree.h"

namespace fixedge {

// A split's id (its SplitTabId token). A layout is the split's own, kept under
// it: it follows the split torn off into another window (where a layout kept
// per window was lost, a 2x2 coming out as a row), and two grown splits shown
// in turn in one window keep theirs.
using SplitKey = std::array<uint8_t, sizeof(edge::OptionalSplitId::token)>;

inline std::map<SplitKey, Node>& Trees() {
  static std::map<SplitKey, Node> trees;
  return trees;
}

// A tree set while its window shows no split yet (one being created): it waits
// under the window's MultiContentsView until the split is there to take it.
inline std::map<void*, Node>& PendingTrees() {
  static std::map<void*, Node> trees;
  return trees;
}

// The split `mcv` shows, if any.
inline std::optional<SplitKey> ShownSplit(void* mcv) {
  void* tsm = mcv ? edge::McvTabStripModel(mcv) : nullptr;
  void* tab = tsm ? edge::ActiveTab(tsm) : nullptr;
  const bool isSplitShown = tab && edge::TabIsSplit(tab);
  if (!isSplitShown) {
    return std::nullopt;
  }
  const edge::OptionalSplitId splitId = edge::TabSplitId(tab);
  if (!splitId.hasValue) {
    return std::nullopt;
  }
  SplitKey key{};
  std::memcpy(key.data(), splitId.token, key.size());
  return key;
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

// A pane dragged out over the tab list is laid out as if it had left the split
// (the rest close up), until the drop makes that real or Esc puts it back.
// One drag at a time, so one pane.
struct HeldOutPane {
  void* mcv = nullptr;
  int pane = -1;
};
inline constinit HeldOutPane g_heldOut;

// The tree of the split `mcv` shows. A pending tree passes to the first split
// shown with none of its own: one being created, or the same split under the
// new id Edge gives it when it grows (HandOverTree).
inline const Node* TreeFor(void* mcv) {
  const std::optional<SplitKey> key = ShownSplit(mcv);
  const auto pending = PendingTrees().find(mcv);
  const bool hasPending = pending != PendingTrees().end();
  if (!key) {
    return hasPending ? &pending->second : nullptr;
  }
  auto entry = Trees().find(*key);
  const bool isAdopting = hasPending && entry == Trees().end();
  if (isAdopting) {
    entry = Trees().insert_or_assign(*key, std::move(pending->second)).first;
    PendingTrees().erase(pending);
  }
  return entry == Trees().end() ? nullptr : &entry->second;
}

inline void SetTree(void* mcv, Node tree) {
  const std::optional<SplitKey> key = ShownSplit(mcv);
  if (key) {
    Trees()[*key] = std::move(tree);
    PendingTrees().erase(mcv);
  } else {
    PendingTrees()[mcv] = std::move(tree);
  }
}

// Before an edit that rebuilds the split: AddToSplitImpl gives the grown split
// a new id, so the tree is set under the id it has now and also waits for the
// new one (TreeFor). EndTreeHandOver closes the hand-over once the edit is done.
inline void HandOverTree(void* mcv, const Node& tree) {
  SetTree(mcv, tree);
  PendingTrees()[mcv] = tree;
}

// The rebuilt split has read its tree by now (the grow's relayout); a hand-over
// left pending would pass to the next split shown without a tree.
inline void EndTreeHandOver(void* mcv) {
  TreeFor(mcv);
  PendingTrees().erase(mcv);
}

// The split view is being destroyed. Its splits' trees stay: they are the
// splits' own, and a split torn off lives on in another window.
inline void ForgetSplitView(void* mcv) {
  PendingTrees().erase(mcv);
  if (g_activeMcv == mcv) {
    g_activeMcv = nullptr;
  }
}

inline void ForgetTree(void* mcv) {
  const std::optional<SplitKey> key = ShownSplit(mcv);
  if (key) {
    Trees().erase(*key);
  }
  PendingTrees().erase(mcv);
}

}  // namespace fixedge
