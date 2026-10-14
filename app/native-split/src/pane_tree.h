// pane_tree.h — the pure split-layout model. One concern, zero Edge/Win32 deps,
// so it is unit-testable in isolation (mirrors pade's pure reorder.ts). The tree
// is the single source of truth for where each pane sits; the geometry detour
// only translates its output into Edge ChildLayouts.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "geom.h"

namespace fixedge {

// A node splits its area among its children along one axis. Horizontal = children
// laid left->right; Vertical = stacked top->bottom. A leaf owns one container
// (by index into the MCV container vector).
enum class Axis : uint8_t {
  kHorizontal,
  kVertical
};

struct Node {
  bool is_leaf = true;
  int leaf = -1;                  // container index, when a leaf
  Axis axis = Axis::kHorizontal;  // split direction, when internal
  std::vector<Node> children;     // internal children, in order
  std::vector<float> weights;     // parallel to children; relative sizes
};

struct LeafRect {
  int leaf;
  Rect rect;
};

// The per-pane minimum length along the split axis (Edge floors panes similarly,
// so a resize can't crush one to nothing). Capped below by span/n so a small
// window shrinks panes evenly instead of overflowing.
inline constexpr int kDesiredMinPane = 64;

// Recursively place every leaf inside `bounds`, leaving `gap` px between siblings.
// Pure; early-returns on degenerate input. Each pane keeps at least a minimum
// length, and the extra is shared by weight with the last child absorbing
// rounding, so the panes exactly tile the bounds with no seam drift. Because the
// lengths come from `bounds`, the same tree re-laid in a resized window reflows
// proportionally — resize handling is intrinsic, not a special case.
inline void LayoutTree(const Node& node, const Rect& bounds, int gap,
                       std::vector<LeafRect>& out) {
  if (node.is_leaf) {
    out.push_back({node.leaf, bounds});
    return;
  }
  const int n = static_cast<int>(node.children.size());
  if (n == 0) {
    return;
  }

  float total = 0.f;
  for (int i = 0; i < n; ++i) {
    total +=
        (static_cast<size_t>(i) < node.weights.size() ? node.weights[i] : 1.f);
  }
  if (total <= 0.f) {
    total = static_cast<float>(n);
  }

  const bool horiz = node.axis == Axis::kHorizontal;
  const int span = std::max(0, (horiz ? bounds.w : bounds.h) - gap * (n - 1));
  // Feasible floor: never larger than an equal share, so the pins always fit.
  const int min_len = std::min(kDesiredMinPane, span / n);

  std::vector<float> w(n);
  for (int i = 0; i < n; ++i) {
    w[i] =
        (static_cast<size_t>(i) < node.weights.size() ? node.weights[i] : 1.f);
  }

  // Water-fill: pin to min_len only the panes whose proportional length would
  // fall below it; the rest keep their exact ratio of the remaining span.
  std::vector<bool> pinned(n, false);
  for (;;) {
    float free_w = 0.f;
    int pinned_count = 0;
    for (int i = 0; i < n; ++i) {
      if (pinned[i]) {
        ++pinned_count;
      } else {
        free_w += w[i];
      }
    }
    const int free_span = span - min_len * pinned_count;
    bool changed = false;
    if (free_w > 0.f) {
      for (int i = 0; i < n; ++i) {
        if (!pinned[i] && static_cast<float>(free_span) * (w[i] / free_w) <
                              static_cast<float>(min_len)) {
          pinned[i] = true;
          changed = true;
        }
      }
    }
    if (!changed) {
      break;
    }
  }

  float free_w = 0.f;
  int pinned_count = 0;
  for (int i = 0; i < n; ++i) {
    if (pinned[i]) {
      ++pinned_count;
    } else {
      free_w += w[i];
    }
  }
  const int free_span = span - min_len * pinned_count;

  int cursor = horiz ? bounds.x : bounds.y;
  int used = 0;
  for (int i = 0; i < n; ++i) {
    int len;
    if (i == n - 1) {
      len = span - used;  // last absorbs rounding so panes tile exactly
    } else if (pinned[i]) {
      len = min_len;
    } else {
      len = free_w > 0.f ? static_cast<int>(static_cast<float>(free_span) *
                                            (w[i] / free_w))
                         : min_len;
    }
    used += len;
    const Rect child = horiz ? Rect{cursor, bounds.y, len, bounds.h}
                             : Rect{bounds.x, cursor, bounds.w, len};
    LayoutTree(node.children[i], child, gap, out);
    cursor += len + gap;
  }
}

// A flat equal split of `count` leaves along `axis` — the proven behaviour and
// the fallback until the UI reshapes the tree.
inline Node FlatTree(int count, Axis axis) {
  Node root;
  root.is_leaf = false;
  root.axis = axis;
  for (int i = 0; i < count; ++i) {
    Node leaf;
    leaf.is_leaf = true;
    leaf.leaf = i;
    root.children.push_back(std::move(leaf));
    root.weights.push_back(1.f);
  }
  return root;
}

// --- pure tree edits (the Arc behaviour: directional add, swap) ---------------

// The four directions a pane can be grown in. Up/Down split along the vertical
// axis (stacked); Left/Right along the horizontal. Up/Left put the new pane
// before the target; Down/Right after it.
enum class Direction : uint8_t {
  kUp,
  kDown,
  kLeft,
  kRight
};

inline Axis AxisOf(Direction d) {
  return (d == Direction::kUp || d == Direction::kDown) ? Axis::kVertical
                                                        : Axis::kHorizontal;
}
inline bool IsBefore(Direction d) {
  return d == Direction::kUp || d == Direction::kLeft;
}

inline Node MakeLeaf(int leaf) {
  Node n;
  n.is_leaf = true;
  n.leaf = leaf;
  return n;
}

// --- canonical form + weights -------------------------------------------------
// After every edit the tree is canonical: every split has >= 2 children, no split
// shares its parent's axis, and each split's weights are > 0 and sum to its child
// count (so 1.0 means "an average share"). The approach is the one shared by
// VS Code's grid, dockview and react-mosaic (see docs/re/drag-dock.md).

inline void FixWeights(Node& n) {
  n.weights.resize(n.children.size(), 1.f);
  float sum = 0.f;
  for (float& w : n.weights) {
    if (!(w > 0.f) || !std::isfinite(w)) {
      w = 1.f;
    }
    sum += w;
  }
  const float k = static_cast<float>(n.children.size()) / sum;
  for (float& w : n.weights) {
    w *= k;
  }
}
inline bool AllEqual(const std::vector<float>& w) {
  float lo = w[0], hi = w[0];
  for (float x : w) {
    lo = std::min(lo, x);
    hi = std::max(hi, x);
  }
  return hi - lo <= 1e-3f * hi;
}
inline float Mean(const std::vector<float>& w) {
  float sum = 0.f;
  for (float x : w) {
    sum += x;
  }
  return sum / static_cast<float>(w.size());
}

// Bottom-up and idempotent: drop empty splits, replace a 1-child split by its
// child (which inherits the slot), and splice a same-axis child split into its
// parent with weights scaled by the child's slot, so on-screen sizes are kept.
inline void Normalize(Node& n) {
  if (n.is_leaf) {
    return;
  }
  FixWeights(n);
  std::vector<Node> kids;
  std::vector<float> ws;
  for (size_t i = 0; i < n.children.size(); ++i) {
    Node& c = n.children[i];
    const float w = n.weights[i];
    Normalize(c);
    if (!c.is_leaf && c.children.empty()) {
      continue;
    }
    if (!c.is_leaf && c.axis == n.axis) {
      float cs = 0.f;
      for (float x : c.weights) {
        cs += x;
      }
      for (size_t j = 0; j < c.children.size(); ++j) {
        kids.push_back(std::move(c.children[j]));
        ws.push_back(w * c.weights[j] / cs);
      }
    } else {
      kids.push_back(std::move(c));
      ws.push_back(w);
    }
  }
  n.children = std::move(kids);
  n.weights = std::move(ws);
  if (n.children.size() == 1) {
    Node only = std::move(n.children[0]);
    n = std::move(only);
    return;
  }
  if (!n.children.empty()) {
    FixWeights(n);
  }
}

// --- lookup -------------------------------------------------------------------

// The split directly holding `leaf`, and its index there. Leaves are found by
// id, never by a stored path, so a lookup after an edit is always fresh.
struct Slot {
  Node* parent = nullptr;
  size_t index = 0;
  bool found = false;
};
inline Slot FindSlot(Node& node, int leaf) {
  for (size_t i = 0; i < node.children.size(); ++i) {
    Node& c = node.children[i];
    if (c.is_leaf && c.leaf == leaf) {
      return {&node, i, true};
    }
    if (!c.is_leaf) {
      Slot s = FindSlot(c, leaf);
      if (s.found) {
        return s;
      }
    }
  }
  return {};
}
inline bool Contains(const Node& n, int leaf) {
  if (n.is_leaf) {
    return n.leaf == leaf;
  }
  for (const Node& c : n.children) {
    if (Contains(c, leaf)) {
      return true;
    }
  }
  return false;
}
inline Node MakeSplit(Axis axis, Node first, Node second) {
  Node s;
  s.is_leaf = false;
  s.axis = axis;
  s.children.push_back(std::move(first));
  s.children.push_back(std::move(second));
  s.weights = {1.f, 1.f};
  return s;
}

// --- edits ----------------------------------------------------------------------

// Place `leaf` next to `target` on side `dir`. If the target's split already runs
// along that axis the leaf joins it: an equal row stays equal, a resized row only
// gives up half of the target. Otherwise the target is wrapped in a new 50/50
// split that keeps the target's slot. Returns false if the target is missing.
inline bool InsertBeside(Node& root, int target, Direction dir, int leaf) {
  const Axis want = AxisOf(dir);
  const bool before = IsBefore(dir);
  if (root.is_leaf) {
    if (root.leaf != target) {
      return false;
    }
    Node old = std::move(root);
    root = before ? MakeSplit(want, MakeLeaf(leaf), std::move(old))
                  : MakeSplit(want, std::move(old), MakeLeaf(leaf));
    return true;
  }
  Slot s = FindSlot(root, target);
  if (!s.found) {
    return false;
  }
  Node& p = *s.parent;
  if (p.axis == want) {
    const size_t at = before ? s.index : s.index + 1;
    float w;
    if (AllEqual(p.weights)) {
      w = Mean(p.weights);
    } else {
      w = p.weights[s.index] / 2.f;
      p.weights[s.index] -= w;
    }
    p.children.insert(p.children.begin() + static_cast<std::ptrdiff_t>(at),
                      MakeLeaf(leaf));
    p.weights.insert(p.weights.begin() + static_cast<std::ptrdiff_t>(at), w);
    FixWeights(p);
  } else {
    Node& t = p.children[s.index];
    Node old = std::move(t);
    t = before ? MakeSplit(want, MakeLeaf(leaf), std::move(old))
               : MakeSplit(want, std::move(old), MakeLeaf(leaf));
  }
  return true;
}

// Add a NEW pane `new_leaf` next to `target_leaf` in `dir` (directional add).
inline bool SplitPane(Node& root, int target_leaf, Direction dir,
                      int new_leaf) {
  if (!InsertBeside(root, target_leaf, dir, new_leaf)) {
    return false;
  }
  Normalize(root);
  return true;
}

// Remove `leaf` from the tree; its weight is dropped so the siblings grow in
// proportion. Refuses to remove the last pane. Returns false if not found.
inline bool Detach(Node& root, int leaf) {
  if (root.is_leaf) {
    return false;
  }
  Slot s = FindSlot(root, leaf);
  if (!s.found) {
    return false;
  }
  s.parent->children.erase(s.parent->children.begin() +
                           static_cast<std::ptrdiff_t>(s.index));
  s.parent->weights.erase(s.parent->weights.begin() +
                          static_cast<std::ptrdiff_t>(s.index));
  Normalize(root);
  return true;
}

// Move child `from` of `p` to insert-before position `to` (a pre-removal index),
// carrying its weight.
inline void MoveChild(Node& p, size_t from, size_t to) {
  if (from < to) {
    --to;
  }
  Node c = std::move(p.children[from]);
  const float w = p.weights[from];
  p.children.erase(p.children.begin() + static_cast<std::ptrdiff_t>(from));
  p.weights.erase(p.weights.begin() + static_cast<std::ptrdiff_t>(from));
  p.children.insert(p.children.begin() + static_cast<std::ptrdiff_t>(to),
                    std::move(c));
  p.weights.insert(p.weights.begin() + static_cast<std::ptrdiff_t>(to), w);
}

// Drop onto a pane's edge: move `moving` next to `target` on side `dir`. Within
// the same split along that axis it is a pure reorder (the pane keeps its size);
// otherwise remove it, then insert beside the target found afresh by id.
// Returns false for a no-op (itself, missing ids, or already there).
inline bool MovePane(Node& root, int moving, int target, Direction dir) {
  if (moving == target || root.is_leaf) {
    return false;
  }
  Slot sm = FindSlot(root, moving), st = FindSlot(root, target);
  if (!sm.found || !st.found) {
    return false;
  }
  if (sm.parent == st.parent && sm.parent->axis == AxisOf(dir)) {
    const size_t to = IsBefore(dir) ? st.index : st.index + 1;
    if (to == sm.index || to == sm.index + 1) {
      return false;
    }
    MoveChild(*sm.parent, sm.index, to);
    return true;
  }
  Detach(root, moving);
  InsertBeside(root, target, dir, moving);
  Normalize(root);
  return true;
}

// Drop onto an outer edge of the whole layout: `moving` spans that edge. A root
// along the other axis is wrapped 50/50 (the pane takes the full half); a root
// along the same axis gains it as an end child with an average share.
// Returns false for a no-op (single pane, missing, or already spanning that edge).
inline bool MovePaneToRootEdge(Node& root, int moving, Direction dir) {
  if (root.is_leaf || !Contains(root, moving)) {
    return false;
  }
  const Axis want = AxisOf(dir);
  const bool first = IsBefore(dir);
  if (root.axis == want) {
    const size_t edge = first ? 0 : root.children.size() - 1;
    for (size_t i = 0; i < root.children.size(); ++i) {
      if (root.children[i].is_leaf && root.children[i].leaf == moving) {
        if (i == edge) {
          return false;
        }
        MoveChild(root, i, first ? 0 : root.children.size());
        return true;
      }
    }
  }
  Detach(root, moving);
  if (!root.is_leaf && root.axis == want) {
    const float w = Mean(root.weights);
    root.children.insert(first ? root.children.begin() : root.children.end(),
                         MakeLeaf(moving));
    root.weights.insert(first ? root.weights.begin() : root.weights.end(), w);
    FixWeights(root);
  } else {
    Node old = std::move(root);
    root = first ? MakeSplit(want, MakeLeaf(moving), std::move(old))
                 : MakeSplit(want, std::move(old), MakeLeaf(moving));
  }
  Normalize(root);
  return true;
}

// Drop onto a pane's centre: the two panes trade places; the sizes stay with the
// positions. Returns false if either is missing or they are the same pane.
inline bool SwapLeaves(Node& node, int a, int b) {
  if (a == b) {
    return false;
  }
  Node* na = nullptr;
  Node* nb = nullptr;
  std::vector<Node*> stack{&node};
  while (!stack.empty()) {
    Node* n = stack.back();
    stack.pop_back();
    if (n->is_leaf) {
      if (n->leaf == a) {
        na = n;
      }
      if (n->leaf == b) {
        nb = n;
      }
    } else {
      for (Node& c : n->children) {
        stack.push_back(&c);
      }
    }
  }
  if (!na || !nb) {
    return false;
  }
  std::swap(na->leaf, nb->leaf);
  return true;
}

// The axis of the split a leaf belongs to — i.e. which way its swap toggle faces
// (vertical parent → top/bottom swap, horizontal → left/right). Searches for the
// parent that directly holds the leaf. Returns kHorizontal if not found.
inline bool SwapAxisIn(const Node& node, int leaf, Axis& out) {
  for (const Node& child : node.children) {
    if (child.is_leaf && child.leaf == leaf) {
      out = node.axis;
      return true;
    }
    if (!child.is_leaf && SwapAxisIn(child, leaf, out)) {
      return true;
    }
  }
  return false;
}
inline Axis SwapAxisOf(const Node& root, int leaf) {
  Axis a = Axis::kHorizontal;
  SwapAxisIn(root, leaf, a);
  return a;
}

// Swap a leaf with its adjacent sibling along the parent's axis (its "counterpart"
// in the spec). Returns true if a swap happened. Pure — operates on the tree's
// leaf indices, so the caller does not move any Edge object.
inline bool SwapWithCounterpart(Node& node, int leaf) {
  for (size_t i = 0; i < node.children.size(); ++i) {
    if (node.children[i].is_leaf && node.children[i].leaf == leaf) {
      // Prefer the next sibling, else the previous — the counterpart on the axis.
      if (i + 1 < node.children.size()) {
        std::swap(node.children[i], node.children[i + 1]);
        return true;
      }
      if (i > 0) {
        std::swap(node.children[i], node.children[i - 1]);
        return true;
      }
      return false;
    }
    if (!node.children[i].is_leaf &&
        SwapWithCounterpart(node.children[i], leaf)) {
      return true;
    }
  }
  return false;
}

// Collect leaf indices left-to-right / top-to-bottom. Pure.
inline void CollectLeaves(const Node& node, std::vector<int>& out) {
  if (node.is_leaf) {
    out.push_back(node.leaf);
    return;
  }
  for (const Node& child : node.children) {
    CollectLeaves(child, out);
  }
}

}  // namespace fixedge
