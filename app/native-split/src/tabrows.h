// tabrows.h — how the tab list shows a split's tabs. Edge's tab strip layout
// (TabStripLayoutHelper, shared by the horizontal strip and the vertical list)
// draws a split as ONE row of two tabs; every member after the pair is given the
// second half's rect and they pile up there. Also a hit-test dump of the views
// under a point (channel kDumpTabRows). UI thread only.
#pragma once
#include <windows.h>
#include <algorithm>
#include <vector>
#include "edge.h"
#include "log.h"
#include "pane_tree.h"
#include "split_data.h"
#include "state.h"

namespace fixedge::tabrows {

inline constexpr int kPairedMembers = split_data::kMinSplitMembers;
inline constexpr int kNoDetachPreview = -1;

// --- the ideal bounds of a split bigger than a pair -------------------------

// Room for the icon, a few letters and the close button.
inline constexpr int kMinSlotWidth = 100;
// The gap between rows when nothing follows the split to measure it from.
inline constexpr int kDefaultRowGap = 4;

// One layout pass's rects; a slot that holds no tab has a null tab view.
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

// The members of the split whose first tab view is `tabView`, by where their
// panes are on screen (ReadingOrder); empty, for strip order, while the split
// has no layout of its own: a flat split's panes already follow it. Member i
// is pane i, since Edge feeds its panes in strip order.
inline std::vector<int> PaneOrder(void* tabView, size_t length) {
  const Node* tree = TreeOfSplit(edge::TabViewSplitToken(tabView));
  const bool isTreeOfRun = tree && std::cmp_equal(LeafCount(*tree), length);
  return isTreeOfRun ? ReadingOrder(*tree) : std::vector<int>{};
}

// `order`: which member each slot takes, in reading order of the slots (rows
// top to bottom, slots left to right); empty for strip order.
inline void SpreadRun(const BoundsList& list, size_t start, size_t length,
                      int rowLeft, int rowRight, int perRow, int rowPitch,
                      const std::vector<int>& order) {
  const Rect first = list.RectAt(start);
  const int rowWidth = rowRight - rowLeft;
  const int rows = (static_cast<int>(length) + perRow - 1) / perRow;
  int placed = 0;
  for (int row = 0; row < rows; row++) {
    // Rows share the tabs out evenly, earlier rows taking the remainder.
    const int remaining = static_cast<int>(length) - placed;
    const int slots = (remaining + (rows - row) - 1) / (rows - row);
    for (int slot = 0; slot < slots; slot++) {
      const int position = placed + slot;
      const int member = order.empty() ? position : order[position];
      Rect& rect = list.RectAt(start + static_cast<size_t>(member));
      rect.x = rowLeft + rowWidth * slot / slots;
      rect.width = rowLeft + rowWidth * (slot + 1) / slots - rect.x;
      rect.y = first.y + row * rowPitch;
      rect.height = first.height;
    }
    placed += slots;
  }
}

// Spreads a piled-up split over as many rows as it needs, at the strip's own row
// pitch, and pushes every later tab down. The container's preferred height is
// computed from these rects too, so this fixes both.
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
    SpreadRun(list, index, length, rowLeft, rowRight, perRow, rowPitch,
              PaneOrder(list.TabAt(index), length));
    pushedDown += (rows - 1) * rowPitch;
    Log("[tabrows] spread a %zu-pane split over %d row(s), %d per row", length,
        rows, perRow);
    index += length;
  }
}

// --- a split's tabs being dragged ---------------------------------------------

// EdgeTabDragContextImpl::CalculateBoundsForDraggedViews draws a dragged split
// as a pair too, and a grown split's later members each on a row of their own:
// they are spread over rows as the strip spreads them, at the pitch Edge put
// between its rows.
inline void SpreadDraggedSplit(void* views, void* bounds) {
  const size_t count = edge::DraggedViewCount(views);
  const bool isOneRectPerView = edge::RectVectorCount(bounds) == count;
  if (!isOneRectPerView || count <= static_cast<size_t>(kPairedMembers)) {
    return;
  }
  void* const* tabViews = edge::DraggedViews(views);
  const bool isSplitTab =
      edge::IsEdgeTabView(tabViews[0]) && edge::IsTabViewInSplit(tabViews[0]);
  if (!isSplitTab) {
    return;
  }
  for (size_t i = 1; i < count; i++) {
    const bool isSameSplit =
        edge::IsTabViewInSameSplit(tabViews[0], tabViews[i]);
    if (!isSameSplit) {
      return;
    }
  }
  const BoundsList list{nullptr, edge::RectVectorBegin(bounds), count};
  int rowLeft = list.RectAt(0).x;
  int rowRight = rowLeft;
  int top = list.RectAt(0).y;
  int rowPitch = 0;
  for (size_t i = 0; i < count; i++) {
    const Rect& rect = list.RectAt(i);
    rowLeft = std::min(rowLeft, rect.x);
    rowRight = std::max(rowRight, rect.x + rect.width);
    top = std::min(top, rect.y);
  }
  for (size_t i = 0; i < count; i++) {
    const int below = list.RectAt(i).y - top;
    const bool isNearerRow = below > 0 && (rowPitch == 0 || below < rowPitch);
    if (isNearerRow) {
      rowPitch = below;
    }
  }
  const int rowHeight = list.RectAt(0).height;
  if (rowPitch == 0) {
    rowPitch = rowHeight + kDefaultRowGap;
  }
  list.RectAt(0).y = top;
  const int fitting = std::max(1, (rowRight - rowLeft) / kMinSlotWidth);
  const int perRow = std::min(static_cast<int>(count), fitting);
  SpreadRun(list, 0, count, rowLeft, rowRight, perRow, rowPitch,
            PaneOrder(tabViews[0], count));
}

using DraggedBounds_t = void*(__fastcall*)(void*, void*, void*);
inline constinit DraggedBounds_t o_dragged_bounds = nullptr;
inline void* __fastcall h_dragged_bounds(void* context, void* bounds,
                                         void* views) {
  void* result = o_dragged_bounds(context, bounds, views);
  SpreadDraggedSplit(views, bounds);
  return result;
}

// --- a pane being dragged out of its split, previewed in the tab list --------

// The strip index of the tab of the pane being dragged out of its split, or -1.
// While it is set, the tab list is laid out as if that tab had already left its
// split: the split's other tabs close up and the tab stands on its own.
inline constinit int g_previewTabIndex = kNoDetachPreview;
// The parent of the last layout pass's tab views. Only ever used within one
// drag.
inline constinit void* g_listView = nullptr;
// The tab view standing on its own in the preview. Every tab of the active split
// is drawn as the active tab, so this one reports itself inactive (h_is_active)
// and draws as a plain tab would.
inline constinit void* g_previewTab = nullptr;
// The gap between a split and a tab of its own in a horizontal strip.
inline constexpr int kPreviewGapDip = 10;

// Its colours (title ink included) and fill follow its new state at once. The
// colours are the EdgeTab's, not its view's: passing the view as `this` crashed
// Edge.
inline void RepaintPreviewTab(void* tab) {
  const bool isEdgeTab = tab && edge::IsEdgeTabView(tab);
  if (!isEdgeTab) {
    return;
  }
  using Fn = void(__fastcall*)(void*);
  reinterpret_cast<Fn>(edge::At(edge155::rva::EdgeTab_UpdateForegroundColors))(
      edge::TabObjectOfView(tab));
  edge::SchedulePaint(tab);
}

// A tab closed during the drag is gone: only a tab still in the list (directly,
// or in a split's row) is touched.
inline bool IsInList(void* tab) {
  if (!g_listView) {
    return false;
  }
  const size_t count = edge::ViewChildCount(g_listView);
  for (size_t i = 0; i < count; i++) {
    void* child = edge::ViewChildAt(g_listView, i);
    if (child == tab) {
      return true;
    }
    const size_t rowCount = edge::ViewChildCount(child);
    for (size_t j = 0; j < rowCount; j++) {
      if (edge::ViewChildAt(child, j) == tab) {
        return true;
      }
    }
  }
  return false;
}

inline void SetPreviewTab(void* tab) {
  if (tab == g_previewTab) {
    return;
  }
  void* previous = g_previewTab;
  g_previewTab = tab;
  if (previous && IsInList(previous)) {
    RepaintPreviewTab(previous);
  }
  RepaintPreviewTab(tab);
}

// bool Tab::IsActive(this): `this` is the tab object, not its view. Only the
// focused pane's tab of a split paints as active, however many panes it has;
// the others paint as its unfocused members. Edge's own answer for a split tab
// is "it or one neighbour is active" (offsets.h SLOTCONTROLLER_vtbl_IsActiveTab),
// which in a grown split lit a sliding pair.
using IsActive_t = bool(__fastcall*)(void*);
inline constinit IsActive_t o_is_active = nullptr;
inline bool __fastcall h_is_active(void* tab) {
  const bool isPreviewTab =
      g_previewTab && tab == edge::TabObjectOfView(g_previewTab);
  if (isPreviewTab) {
    return false;
  }
  if (edge::IsTabObjectInSplit(tab)) {
    return edge::IsTheActiveTabObject(tab);
  }
  return o_is_active(tab);
}

struct DetachLayout {
  size_t slot = 0;
  size_t runStart = 0;  // its split's slots: [runStart, runEnd)
  size_t runEnd = 0;
};

// The slot count when there is no such tab.
inline size_t SlotOfTab(const BoundsList& list, int tabIndex) {
  int tabsSeen = 0;
  for (size_t index = 0; index < list.count; index++) {
    if (list.TabAt(index)) {
      if (tabsSeen == tabIndex) {
        return index;
      }
      tabsSeen++;
    }
  }
  return list.count;
}

// How far along the list's axis a rect reaches: its top in a vertical list, its
// right edge in a horizontal strip.
inline int ReachOf(const Rect& rect, bool isVertical) {
  return isVertical ? rect.y : rect.x + rect.width;
}

struct RunRects {
  std::vector<Rect> before;
  int left = 0;
  int right = 0;
  int reach = 0;  // the furthest ReachOf any of them has
};

inline RunRects MeasureRun(const BoundsList& list, const DetachLayout& detach,
                           bool isVertical) {
  RunRects run;
  run.before.assign(list.rects + detach.runStart, list.rects + detach.runEnd);
  run.left = run.before.front().x;
  run.right = run.left;
  run.reach = ReachOf(run.before.front(), isVertical);
  for (const Rect& rect : run.before) {
    run.left = std::min(run.left, rect.x);
    run.right = std::max(run.right, rect.x + rect.width);
    run.reach = std::max(run.reach, ReachOf(rect, isVertical));
  }
  return run;
}

inline void CloseUp(const BoundsList& list, const DetachLayout& detach,
                    const RunRects& run) {
  size_t next = 0;
  for (size_t slot = detach.runStart; slot < detach.runEnd; slot++) {
    if (slot != detach.slot) {
      list.RectAt(slot) = run.before[next++];
    }
  }
}

// Returns how far the shifted tabs then reach, starting from `reach`.
inline int ShiftRemaining(const BoundsList& list, const DetachLayout& detach,
                          int offset, bool isVertical, int reach) {
  for (size_t slot = detach.runStart; slot < detach.runEnd; slot++) {
    if (slot == detach.slot) {
      continue;
    }
    Rect& rect = list.RectAt(slot);
    (isVertical ? rect.y : rect.x) += offset;
    reach = std::max(reach, ReachOf(rect, isVertical));
  }
  return reach;
}

// A vertical list: the leaving tab takes a full row, after the split's rows if it
// was the split's last tab (in the freed row when the last row held it alone),
// else before them, which pushes the others down a row. Returns the reach.
inline int PlaceDetachedVertical(const BoundsList& list,
                                 const DetachLayout& detach,
                                 const RunRects& run, bool isLast,
                                 Rect& detached) {
  const Rect first = run.before.front();
  const Rect last = run.before.back();
  const bool hasFollowing = detach.runEnd < list.count;
  const int rowGap = hasFollowing
                         ? list.RectAt(detach.runEnd).y - (last.y + last.height)
                         : kDefaultRowGap;
  const int pitch = last.height + std::max(rowGap, 0);
  detached.x = run.left;
  detached.width = run.right - run.left;
  if (!isLast) {
    return ShiftRemaining(list, detach, pitch, true, first.y);
  }
  const bool isLastRowFree = run.before[run.before.size() - 2].y != last.y;
  detached.y = isLastRowFree ? last.y : last.y + pitch;
  return detached.y;
}

// A horizontal strip: the leaving tab takes a slot a gap away from the split's
// others, after them if it was the split's last tab, else before them, which
// pushes the others along. Returns the reach.
inline int PlaceDetachedHorizontal(const BoundsList& list,
                                   const DetachLayout& detach,
                                   const RunRects& run, bool isLast,
                                   Rect& detached) {
  const Rect first = run.before.front();
  if (isLast) {
    detached.x = run.before.back().x + kPreviewGapDip;
    return detached.x + detached.width;
  }
  const int offset = first.width + kPreviewGapDip;
  return ShiftRemaining(list, detach, offset, false, first.x + first.width);
}

// The leaving tab stands after the others if it was the split's last tab, else
// before them: the tab list sizes itself from the last tab's rect, so that tab
// must stay the furthest on.
inline void LayOutDetached(const BoundsList& list, const DetachLayout& detach,
                           bool isVertical) {
  const RunRects run = MeasureRun(list, detach, isVertical);
  CloseUp(list, detach, run);
  const bool isLast = detach.slot == detach.runEnd - 1;
  Rect detached = run.before.front();
  const int newReach =
      isVertical ? PlaceDetachedVertical(list, detach, run, isLast, detached)
                 : PlaceDetachedHorizontal(list, detach, run, isLast, detached);
  list.RectAt(detach.slot) = detached;
  const int shift = newReach - run.reach;
  for (size_t i = detach.runEnd; i < list.count; i++) {
    (isVertical ? list.RectAt(i).y : list.RectAt(i).x) += shift;
  }
}

// A pair is Edge's to take apart, so only a grown split gets the preview.
inline void ApplyDetachPreview(void* helper, void* result) {
  const BoundsList list{helper, edge::IdealBoundsRects(result),
                        edge::IdealBoundsCount(result)};
  const bool hasRectPerSlot =
      list.rects && list.count == edge::TabLayoutSlotCount(helper);
  if (!hasRectPerSlot) {
    return;
  }
  for (size_t i = 0; i < list.count && !g_listView; i++) {
    void* firstTab = list.TabAt(i);
    g_listView = firstTab ? edge::ViewParent(firstTab) : nullptr;
  }
  if (g_previewTabIndex < 0) {
    return;
  }
  DetachLayout detach;
  detach.slot = SlotOfTab(list, g_previewTabIndex);
  if (detach.slot >= list.count) {
    return;
  }
  void* tab = list.TabAt(detach.slot);
  detach.runStart = detach.slot;
  while (detach.runStart > 0 && list.TabAt(detach.runStart - 1) &&
         edge::IsTabViewInSameSplit(tab, list.TabAt(detach.runStart - 1))) {
    detach.runStart--;
  }
  const size_t length = SplitRunLength(list, detach.runStart);
  detach.runEnd = detach.runStart + length;
  const bool isGrownSplit = length > static_cast<size_t>(kPairedMembers);
  if (!isGrownSplit) {
    return;
  }
  LayOutDetached(list, detach, edge::IsVerticalTabView(tab));
  SetPreviewTab(edge::IsEdgeTabView(tab) ? tab : nullptr);
}

// `kNoDetachPreview` ends the preview. Pass `isRelayoutWanted` false when the real
// change is about to relayout anyway.
inline void SetDetachPreview(int tabIndex, bool isRelayoutWanted = true) {
  if (tabIndex == g_previewTabIndex) {
    return;
  }
  g_previewTabIndex = tabIndex;
  Log("[tabrows] detach preview of tab %d (list view %p, relayout %d)",
      tabIndex, g_listView, isRelayoutWanted ? 1 : 0);
  if (g_listView && isRelayoutWanted) {
    edge::InvalidateLayout(g_listView);
    edge::LayoutImmediately(g_listView);
  }
  if (tabIndex < 0) {
    g_listView = nullptr;
    SetPreviewTab(nullptr);
  }
}

// TabStripLayoutHelper::CalculateIdealBounds(this, out, optional<int> width):
// fixed up before the helper stores the bounds or sizes the container from them.
using CalculateIdealBounds_t = void*(__fastcall*)(void*, void*, uint64_t);
inline constinit CalculateIdealBounds_t o_calculate_ideal_bounds = nullptr;
inline void* __fastcall h_calculate_ideal_bounds(void* helper, void* out,
                                                 uint64_t availableWidth) {
  void* result = o_calculate_ideal_bounds(helper, out, availableWidth);
  SpreadPiledSplits(helper, out);
  ApplyDetachPreview(helper, out);
  return result;
}

// --- diagnostic: the views under a point -----------------------------------

inline constexpr int kMaxDepth = 64;
inline constexpr size_t kMaxChildrenListed = 12;
// The point probed by the dump: inside the first row of the vertical tab list.
inline constexpr int kProbeX = 60;
inline constexpr int kProbeY = 104;

// Logs each view's vtable (name it with tools/re/symbols.py), so the class that
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
