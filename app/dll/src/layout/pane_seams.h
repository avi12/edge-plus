// pane_seams.h — the seams between panes, where the dividers sit: each found
// in the laid-out tree, one dragged to trade length between its two panes,
// and a split evened out.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>
#include "edge/geom.h"
#include "layout/pane_layout.h"
#include "layout/pane_normalize.h"
#include "layout/pane_tree.h"

namespace edgeplus {

// The strip between child `index` and `index + 1` of the split at `path`
// (child indices from the root). For a horizontal split it is a column from
// x = `position`; for a vertical split, a row from y = `position`.
// `lengthBefore` / `lengthAfter` are the two children's lengths when measured.
struct Seam {
  std::vector<int> path;
  int index = 0;
  Axis axis = Axis::kHorizontal;
  int position = 0;
  int thickness = 0;
  int crossStart = 0;
  int crossLength = 0;
  int lengthBefore = 0;
  int lengthAfter = 0;
};

inline void CollectSeams(const Node& node, const Rect& bounds, int gap,
                         std::vector<int>& path, std::vector<Seam>& out) {
  if (node.isLeaf) {
    return;
  }
  const std::vector<Rect> rects = ChildRects(node, bounds, gap);
  const bool isHorizontal = node.axis == Axis::kHorizontal;
  for (size_t i = 0; i + 1 < rects.size(); i++) {
    Seam seam;
    seam.path = path;
    seam.index = static_cast<int>(i);
    seam.axis = node.axis;
    seam.position = isHorizontal ? rects[i].x + rects[i].width
                                 : rects[i].y + rects[i].height;
    seam.thickness = gap;
    seam.crossStart = isHorizontal ? bounds.y : bounds.x;
    seam.crossLength = isHorizontal ? bounds.height : bounds.width;
    seam.lengthBefore = isHorizontal ? rects[i].width : rects[i].height;
    seam.lengthAfter = isHorizontal ? rects[i + 1].width : rects[i + 1].height;
    out.push_back(std::move(seam));
  }
  for (size_t i = 0; i < rects.size(); i++) {
    path.push_back(static_cast<int>(i));
    CollectSeams(node.children[i], rects[i], gap, path, out);
    path.pop_back();
  }
}

[[nodiscard]] inline std::vector<Seam> Seams(const Node& root,
                                             const Rect& bounds, int gap) {
  std::vector<Seam> seams;
  std::vector<int> path;
  CollectSeams(root, bounds, gap, path, seams);
  return seams;
}

// Only the two children the seam divides trade length; their combined weight is
// unchanged, so every other proportion holds. `seam` is the measurement taken
// when the drag began, so the result depends only on the total delta.
inline bool ResizeSeam(Node& root, const Seam& seam, int delta) {
  Node* split = NodeAt(root, seam.path);
  const bool isSeamOfSplit =
      (split != nullptr) && !split->isLeaf &&
      static_cast<size_t>(seam.index) + 1 < split->children.size();
  if (!isSeamOfSplit) {
    return false;
  }
  FixWeights(*split);
  const int combinedLength = seam.lengthBefore + seam.lengthAfter;
  if (combinedLength <= 0) {
    return false;
  }
  const int minLength = std::min(kDesiredMinPane, combinedLength / 2);
  const int lengthBefore = std::clamp(seam.lengthBefore + delta, minLength,
                                      combinedLength - minLength);
  float& weightBefore = split->weights[static_cast<size_t>(seam.index)];
  float& weightAfter = split->weights[static_cast<size_t>(seam.index) + 1];
  const float combinedWeight = weightBefore + weightAfter;
  weightBefore = combinedWeight * static_cast<float>(lengthBefore) /
                 static_cast<float>(combinedLength);
  weightAfter = combinedWeight - weightBefore;
  return true;
}

inline bool EqualizeSplit(Node& root, const std::vector<int>& path) {
  Node* split = NodeAt(root, path);
  const bool isUnevenSplit = (split != nullptr) && !split->isLeaf &&
                             !split->weights.empty() &&
                             !AllEqual(split->weights);
  if (!isUnevenSplit) {
    return false;
  }
  std::fill(split->weights.begin(), split->weights.end(), 1.f);
  return true;
}

}  // namespace edgeplus
