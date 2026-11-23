// pane_normalize.h — the tree kept canonical after every edit: its weights
// fixed and evened out, and the splits an edit left empty, single or nested
// along their parent's axis folded away.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>
#include "layout/pane_tree.h"

namespace edgeplus {

// After every edit the tree is canonical: every split has >= 2 children, no split
// shares its parent's axis, and each split's weights are > 0 and sum to its child
// count (so 1.0 means "an average share"); see docs/re/drag-dock.md.

// Relative to the largest weight: float sums drift a little from exact thirds
// and quarters.
inline constexpr float kEqualWeightTolerance = 1e-3f;

inline void FixWeights(Node& node) {
  node.weights.resize(node.children.size(), 1.f);
  float sum = 0.f;
  for (float& weight : node.weights) {
    const bool isValidWeight = weight > 0.f && std::isfinite(weight);
    if (!isValidWeight) {
      weight = 1.f;
    }
    sum += weight;
  }
  const float scale = static_cast<float>(node.children.size()) / sum;
  for (float& weight : node.weights) {
    weight *= scale;
  }
}
[[nodiscard]] inline bool AllEqual(std::span<const float> weights) {
  float lowest = weights[0];
  float highest = weights[0];
  for (const float weight : weights) {
    lowest = std::min(lowest, weight);
    highest = std::max(highest, weight);
  }
  return highest - lowest <= kEqualWeightTolerance * highest;
}
[[nodiscard]] inline float Mean(std::span<const float> weights) {
  float sum = 0.f;
  for (const float weight : weights) {
    sum += weight;
  }
  return sum / static_cast<float>(weights.size());
}

// Bottom-up and idempotent: drop empty splits, replace a 1-child split by its
// child (which inherits the slot), and splice a same-axis child split into its
// parent with weights scaled by the child's slot, so on-screen sizes are kept.
inline void Normalize(Node& node) {
  if (node.isLeaf) {
    return;
  }
  FixWeights(node);
  std::vector<Node> keptChildren;
  std::vector<float> keptWeights;
  for (size_t i = 0; i < node.children.size(); i++) {
    Node& child = node.children[i];
    const float slotWeight = node.weights[i];
    Normalize(child);
    const bool isEmptySplit = !child.isLeaf && child.children.empty();
    if (isEmptySplit) {
      continue;
    }
    const bool isSameAxisSplit = !child.isLeaf && child.axis == node.axis;
    if (isSameAxisSplit) {
      float childWeightSum = 0.f;
      for (const float weight : child.weights) {
        childWeightSum += weight;
      }
      for (size_t j = 0; j < child.children.size(); j++) {
        keptChildren.push_back(std::move(child.children[j]));
        keptWeights.push_back(slotWeight * child.weights[j] / childWeightSum);
      }
    } else {
      keptChildren.push_back(std::move(child));
      keptWeights.push_back(slotWeight);
    }
  }
  node.children = std::move(keptChildren);
  node.weights = std::move(keptWeights);
  const bool hasSingleChild = node.children.size() == 1;
  if (hasSingleChild) {
    // Moved out first: the child lives inside `node`, which the assignment
    // overwrites.
    Node onlyChild = std::move(node.children[0]);
    node = std::move(onlyChild);
    return;
  }
  if (!node.children.empty()) {
    FixWeights(node);
  }
}

}  // namespace edgeplus
