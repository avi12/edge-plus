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
#include <bit>
#include <utility>
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

using ArcTo_t = void*(__fastcall*)(void*, uint64_t, float, int, uint8_t,
                                   uint64_t);
inline constinit ArcTo_t o_arc_to = nullptr;

// An SkPoint, as arcTo takes it by value in one register.
struct SkPoint {
  float x = 0;
  float y = 0;
};

// The four arcs of a horizontal tab's outline in EdgeTabStyleViews::GetPath, in
// the order it draws them (offsets.h), told apart by their return address.
enum class Corner {
  kNone,
  kLeftFoot,
  kTopLeft,
  kTopRight,
  kRightFoot
};

inline Corner CornerOfCaller(const void* caller) {
  const std::pair<uintptr_t, Corner> corners[] = {
      {edge155::rva::EdgeTabStyleViews_GetPath_leftFoot_return,
       Corner::kLeftFoot},
      {edge155::rva::EdgeTabStyleViews_GetPath_topLeftArc_return,
       Corner::kTopLeft},
      {edge155::rva::EdgeTabStyleViews_GetPath_topRightArc_return,
       Corner::kTopRight},
      {edge155::rva::EdgeTabStyleViews_GetPath_rightFoot_return,
       Corner::kRightFoot}};
  for (const auto& [rva, corner] : corners) {
    if (caller == edge::At(rva)) {
      return corner;
    }
  }
  return Corner::kNone;
}

// How far the trailing top corner's cut reaches, as a share of the corner's
// radius: enough to read as a chamfer, short of the close button beside it.
inline constexpr float kTrailingCutRatio = 0.75f;
// The corner radius GetPath is using (the arcs before the top-right one set it).
inline constinit float g_cornerRadius = 0;
// The cut at the top-right corner, in the units of the path.
inline float TrailingCut() {
  return g_cornerRadius * kTrailingCutRatio;
}

// SkPath::arcTo, for the calls in GetPath that round a horizontal tab's outline.
// With no radius an arc is a straight line to its end, so the feet (the flared
// tails either side of the tab) end level with the tab's bottom and the sides
// run straight; the top-left corner is square; and the top-right corner, by the
// close button, becomes a 45 degree cut, the chamfer of the skin's cards and
// buttons (h_line_to starts it earlier along the top). Every other caller is
// Skia's own use, unchanged. A right-to-left UI keeps Edge's corners: its
// leading corner is the right one, built from different calls.
inline void* __fastcall h_arc_to(void* path, uint64_t radii, float rotation,
                                 int arcSize, uint8_t direction, uint64_t end) {
  const Corner corner =
      g_outlinedTab ? CornerOfCaller(_ReturnAddress()) : Corner::kNone;
  const bool isHandled =
      corner != Corner::kNone &&
      (!edge::IsUiRightToLeft() || corner == Corner::kLeftFoot ||
       corner == Corner::kRightFoot);
  if (!isHandled) {
    return o_arc_to(path, radii, rotation, arcSize, direction, end);
  }
  const SkPoint radius = std::bit_cast<SkPoint>(radii);
  g_cornerRadius = radius.x;
  SkPoint target = std::bit_cast<SkPoint>(end);
  switch (corner) {
    case Corner::kLeftFoot:
      target.y += radius.y;
      break;
    case Corner::kTopLeft:
      target.x -= radius.x;
      break;
    case Corner::kTopRight:
      target.y += TrailingCut();
      break;
    case Corner::kRightFoot:
      target.x -= radius.x;
      break;
    case Corner::kNone:
      break;
  }
  return o_arc_to(path, std::bit_cast<uint64_t>(SkPoint{}), rotation, arcSize,
                  direction, std::bit_cast<uint64_t>(target));
}

using LineTo_t = void*(__fastcall*)(void*, uint64_t);
inline constinit LineTo_t o_line_to = nullptr;

// SkPath::lineTo, for the one call in GetPath that draws the tab's top edge up
// to its top-right corner: it stops a cut's length short, where the chamfer
// starts. Every other caller is Skia's own use, unchanged.
inline void* __fastcall h_line_to(void* path, uint64_t point) {
  const bool isTopEdge =
      g_outlinedTab && !edge::IsUiRightToLeft() &&
      _ReturnAddress() ==
          edge::At(edge155::rva::EdgeTabStyleViews_GetPath_topEdge_return);
  if (!isTopEdge) {
    return o_line_to(path, point);
  }
  SkPoint target = std::bit_cast<SkPoint>(point);
  target.x -= TrailingCut();
  return o_line_to(path, std::bit_cast<uint64_t>(target));
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
