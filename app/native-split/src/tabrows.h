// tabrows.h — how the tab list shows a split's tabs. One concern: Edge's tab
// strip layout (TabStripLayoutHelper, shared by the horizontal strip and the
// vertical list) draws a split as ONE row of two tabs side by side. With three
// or more members, every member after the pair is given the second half's rect
// and they pile up there. After the helper computes the ideal bounds, a split of
// more than two members is spread out: as many slots to a row as fit, the rest
// on new rows below (and every later tab pushed down). Also here: a hit-test
// dump of the views under a point (channel kDumpTabRows), for finding which
// class draws a part of the tab list. UI thread only.
#pragma once
#include <windows.h>
#include <algorithm>
#include "edge.h"
#include "log.h"

namespace fixedge::tabrows {

// A tab row pairs only exactly two members; anything bigger is shown unpaired.
inline constexpr int kPairedMembers = 2;

// --- the ideal bounds of a split bigger than a pair -------------------------

// TabStripLayoutHelper::CalculateIdealBounds returns one Rect per slot. A
// split's first two tabs get half a row each and every further member is given
// the second half's rect, so they pile up there. This spreads such a split over
// as many rows as it needs: as many slots to a row as fit at kMinSlotWidth, rows
// stacked at the strip's own row pitch, and every later tab pushed down by the
// extra rows. Both the layout and the container's preferred height are computed
// from this result, so fixing it here fixes both.
// A pane's slot is never narrower than this (icon, a few letters, the close
// button); a row holds as many slots as fit, and the rest go on a new row.
inline constexpr int kMinSlotWidth = 100;
// The gap between rows when nothing follows the split to measure it from.
inline constexpr int kDefaultRowGap = 4;

// The rects of one layout pass, with the tab view of each slot (null for a slot
// that holds no tab).
struct BoundsList {
  void* helper = nullptr;
  Rect* rects = nullptr;
  size_t count = 0;

  [[nodiscard]] void* TabAt(size_t index) const {
    return edge::TabLayoutSlotTab(helper, index);
  }
  [[nodiscard]] Rect& RectAt(size_t index) const {
    return rects[index];
  }
};

inline bool Overlaps(const Rect& first, const Rect& second) {
  return first.x < second.x + second.width &&
         second.x < first.x + first.width &&
         first.y < second.y + second.height &&
         second.y < first.y + first.height;
}

// The run of entries from `start` that belong to the same split as it.
inline size_t SplitRunLength(const BoundsList& list, size_t start) {
  void* first = list.TabAt(start);
  const bool isSplitTab = first && edge::IsTabViewInSplit(first);
  if (!isSplitTab) {
    return 1;
  }
  size_t length = 1;
  while (start + length < list.count) {
    void* next = list.TabAt(start + length);
    const bool isSameSplit = next && edge::IsTabViewInSameSplit(first, next);
    if (!isSameSplit) {
      break;
    }
    length++;
  }
  return length;
}

// Whether any two of the run's rects lie on top of each other (the pile-up).
inline bool IsPiledUp(const BoundsList& list, size_t start, size_t length) {
  for (size_t i = start; i < start + length; i++) {
    for (size_t j = i + 1; j < start + length; j++) {
      if (Overlaps(list.RectAt(i), list.RectAt(j))) {
        return true;
      }
    }
  }
  return false;
}

// Lay `length` tabs out over rows of `perRow` slots filling `rowLeft`..`rowRight`.
inline void SpreadRun(const BoundsList& list, size_t start, size_t length,
                      int rowLeft, int rowRight, int perRow, int rowPitch) {
  const Rect first = list.RectAt(start);
  const int rowWidth = rowRight - rowLeft;
  const int rows = (static_cast<int>(length) + perRow - 1) / perRow;
  int placed = 0;
  for (int row = 0; row < rows; row++) {
    // Rows share the tabs out evenly, earlier rows taking the remainder.
    const int remaining = static_cast<int>(length) - placed;
    const int slots = (remaining + (rows - row) - 1) / (rows - row);
    for (int slot = 0; slot < slots; slot++) {
      Rect& rect = list.RectAt(start + static_cast<size_t>(placed + slot));
      rect.x = rowLeft + rowWidth * slot / slots;
      rect.width = rowLeft + rowWidth * (slot + 1) / slots - rect.x;
      rect.y = first.y + row * rowPitch;
      rect.height = first.height;
    }
    placed += slots;
  }
}

// Fix every piled-up split among `result` (CalculateIdealBounds's rects).
inline void SpreadPiledSplits(void* helper, void* result) {
  const BoundsList list{helper, edge::IdealBoundsRects(result),
                        edge::IdealBoundsCount(result)};
  const bool hasRectPerSlot =
      list.rects && list.count == edge::TabLayoutSlotCount(helper);
  if (!hasRectPerSlot) {
    return;
  }
  int pushedDown = 0;
  for (size_t index = 0; index < list.count;) {
    if (pushedDown != 0) {
      list.RectAt(index).y += pushedDown;
    }
    const size_t length = SplitRunLength(list, index);
    const bool isPiledSplit = length > static_cast<size_t>(kPairedMembers) &&
                              IsPiledUp(list, index, length);
    if (!isPiledSplit) {
      index += length;
      continue;
    }
    // Everything of the run is laid in the strip's own row: its left edge and
    // the right edge of its rightmost tab give the row.
    int rowLeft = list.RectAt(index).x;
    int rowRight = rowLeft;
    for (size_t i = index; i < index + length; i++) {
      const Rect& rect = list.RectAt(i);
      rowLeft = std::min(rowLeft, rect.x);
      rowRight = std::max(rowRight, rect.x + rect.width);
    }
    const int rowHeight = list.RectAt(index).height;
    const bool hasFollowingTab = index + length < list.count;
    const int gap = hasFollowingTab
                        ? list.RectAt(index + length).y -
                              (list.RectAt(index).y + rowHeight) - pushedDown
                        : kDefaultRowGap;
    const int rowPitch = rowHeight + std::max(gap, 0);
    const int fitting = std::max(1, (rowRight - rowLeft) / kMinSlotWidth);
    const int perRow = std::min(static_cast<int>(length), fitting);
    const int rows = (static_cast<int>(length) + perRow - 1) / perRow;
    SpreadRun(list, index, length, rowLeft, rowRight, perRow, rowPitch);
    pushedDown += (rows - 1) * rowPitch;
    Log("[tabrows] spread a %zu-pane split over %d row(s), %d per row", length,
        rows, perRow);
    index += length;
  }
}

// TabStripLayoutHelper::CalculateIdealBounds(this, out, optional<int> width):
// fills `out` with every slot's ideal bounds and returns it. Fix them up before
// the helper stores them or sizes the container from them.
using CalculateIdealBounds_t = void*(__fastcall*)(void*, void*, uint64_t);
inline constinit CalculateIdealBounds_t o_calculate_ideal_bounds = nullptr;
inline void* __fastcall h_calculate_ideal_bounds(void* helper, void* out,
                                                 uint64_t availableWidth) {
  void* result = o_calculate_ideal_bounds(helper, out, availableWidth);
  SpreadPiledSplits(helper, out);
  return result;
}

// --- diagnostic: the views under a point -----------------------------------

inline constexpr int kMaxDepth = 64;
inline constexpr size_t kMaxChildrenListed = 12;
// The point probed by the dump: inside the first row of the vertical tab list.
inline constexpr int kProbeX = 60;
inline constexpr int kProbeY = 104;

// Log every view whose bounds contain `probe` (root coordinates), outermost
// first, with its vtable (name it with tools/re/symbols.py), so the class that
// draws a row can be found without RTTI. `origin` is `view`'s parent's position.
inline void Walk(void* view, int depth, Point origin, Point probe, int* found) {
  if (depth > kMaxDepth) {
    return;
  }
  const Rect bounds = edge::ViewBounds(view);
  const Point position{origin.x + bounds.x, origin.y + bounds.y};
  const bool isUnderProbe =
      probe.x >= position.x && probe.x < position.x + bounds.width &&
      probe.y >= position.y && probe.y < position.y + bounds.height;
  if (!isUnderProbe) {
    return;
  }
  const size_t count = edge::ViewChildCount(view);
  Log("[tabrows] depth %d view %p vtable %#llx at %d,%d %dx%d children %zu",
      depth, view, static_cast<unsigned long long>(edge::VtableRva(view)),
      position.x, position.y, bounds.width, bounds.height, count);
  (*found)++;
  const bool shouldListChildren = count >= 2 && count <= kMaxChildrenListed;
  for (size_t i = 0; shouldListChildren && i < count; i++) {
    void* child = edge::ViewChildAt(view, i);
    const Rect childBounds = edge::ViewBounds(child);
    Log("[tabrows]   child %zu vtable %#llx visible=%d at %d,%d %dx%d", i,
        static_cast<unsigned long long>(edge::VtableRva(child)),
        edge::ViewGetVisible(child) ? 1 : 0, childBounds.x, childBounds.y,
        childBounds.width, childBounds.height);
  }
  for (size_t i = 0; i < count; i++) {
    Walk(edge::ViewChildAt(view, i), depth + 1, position, probe, found);
  }
}

// Log the views under the probe point in `anyView`'s widget.
inline void Dump(void* anyView) {
  if (!anyView) {
    return;
  }
  int found = 0;
  Walk(edge::ViewRoot(anyView), 0, Point{0, 0}, Point{kProbeX, kProbeY},
       &found);
  Log("[tabrows] %d view(s) under %d,%d", found, kProbeX, kProbeY);
}

}  // namespace fixedge::tabrows
