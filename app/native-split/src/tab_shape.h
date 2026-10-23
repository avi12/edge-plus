// tab_shape.h — the vertical tab's outline. One concern: Edge outlines a
// vertical tab (its fill, hover and press) as one rounded rectangle, and a
// split's members' shared fill as another (SplitTabView::OnPaint). While the
// spine is on, both get a straight leading edge (the left one, the right one in
// a right-to-left UI) on every tab and split. Changing the outline is what makes
// the edge straight in whatever colour Edge paints the tab, so nothing is
// painted over the fill. Within a split's row the members' seams are straight
// too, so the row reads as one shape with one round trailing end. Nothing here
// paints, except the group fill: Edge's own drawing, plus the same paint over
// the corners it rounded. UI thread only.
#pragma once
#include <windows.h>
#include <intrin.h>
#include <vector>
#include "edge.h"
#include "geom.h"
#include "tab_edge.h"

namespace fixedge::tab_shape {

inline constinit bool g_isSquared = false;

// The tab EdgeTabStyleViews::GetPath is outlining right now, or null.
inline constinit void* g_outlinedTab = nullptr;

using GetPath_t = void*(__fastcall*)(void*, void*, int, float, const int*);
inline constinit GetPath_t o_get_path = nullptr;

// EdgeTabStyleViews::GetPath: notes which tab it is for, so the
// rounded rectangle it builds can be told from every other (h_set_rect_xy).
inline void* __fastcall h_get_path(void* styleViews, void* outPath,
                                   int pathType, float scale,
                                   const int* isUnscaled) {
  g_outlinedTab =
      g_isSquared ? edge::TabOfStyleViews(styleViews).view : nullptr;
  void* result = o_get_path(styleViews, outPath, pathType, scale, isUnscaled);
  g_outlinedTab = nullptr;
  return result;
}

using SetRectXY_t = void(__fastcall*)(void*, const void*, float, float);
inline constinit SetRectXY_t o_set_rect_xy = nullptr;

// SkRRect::setRectXY, for the one call that builds a vertical tab's outline in
// EdgeTabStyleViews::GetPath: the radius is Edge's, the corners are ours. Every
// other caller is Skia's own use, unchanged.
inline void __fastcall h_set_rect_xy(void* roundedRect, const void* skRect,
                                     float radiusX, float radiusY) {
  const bool isTabOutline =
      g_outlinedTab &&
      _ReturnAddress() ==
          edge::At(edge155::rva::EdgeTabStyleViews_GetPath_setRectXY_return);
  if (!isTabOutline) {
    o_set_rect_xy(roundedRect, skRect, radiusX, radiusY);
    return;
  }
  const tab_edge::Side leading = tab_edge::LeadingSide(edge::IsUiRightToLeft());
  const std::vector<Rect> siblings = edge::SplitSiblingBounds(g_outlinedTab);
  const bool hasTrailingNeighbor =
      tab_edge::HasTrailingNeighbor(edge::ViewBounds(g_outlinedTab), leading,
                                    siblings.data(), siblings.size());
  edge::SetRoundedRectRadii(
      roundedRect, skRect,
      tab_edge::RadiiFor(leading, radiusX, hasTrailingNeighbor));
}

using DrawRoundRect_t = void(__fastcall*)(void*, const Rect*, int, const void*);
inline constinit DrawRoundRect_t o_draw_round_rect = nullptr;

// Canvas::DrawRoundRect, for the one call that paints a split group's rounded
// fill (SplitTabView::OnPaint): Edge draws it, then the same paint goes over the
// leading corners it rounded. Every other caller is Edge's own, unchanged.
inline void __fastcall h_draw_round_rect(void* canvas, const Rect* rect,
                                         int radius, const void* flags) {
  const bool isSplitGroupFill =
      g_isSquared &&
      _ReturnAddress() ==
          edge::At(edge155::rva::SplitTabView_OnPaint_DrawRoundRect_return);
  o_draw_round_rect(canvas, rect, radius, flags);
  if (isSplitGroupFill) {
    const tab_edge::Side leading =
        tab_edge::LeadingSide(edge::IsUiRightToLeft());
    edge::CanvasDrawRect(canvas, tab_edge::LeadingStrip(*rect, leading, radius),
                         flags);
  }
}

}  // namespace fixedge::tab_shape
