// tab_shape.h — the tab's outline. One concern: Edge outlines a tab (its fill,
// hover and press) in EdgeTabStyleViews::GetPath, as one rounded rectangle for a
// vertical tab and as four arcs for a horizontal one, and a split's members'
// shared fill as another rounded rectangle (SplitTabView::OnPaint). While the
// tab shape is on, every tab gets a straight leading edge (the left one, the
// right one in a right-to-left UI) and a 45 degree cut on its trailing top
// corner, the chamfer of the skin's cards and buttons; a horizontal tab also
// loses its flared feet. Changing the outline is what shapes the tab in whatever
// colour Edge paints it, so nothing is painted over the fill. Within a split's
// row the members' seams are straight too, so the row reads as one shape with
// one round trailing end. Nothing here paints, except the group fill: Edge's own
// drawing, plus the same paint over the corners it rounded. UI thread only.
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
  g_outlinedTab = g_isSquared ? edge::TabViewOfStyleViews(styleViews) : nullptr;
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

// The four arcs of a horizontal tab's outline in EdgeTabStyleViews::GetPath, in
// the order it draws them (offsets.h), told apart by their return address.
enum class OutlineArc {
  kNone,
  kLeftFoot,
  kTopLeft,
  kTopRight,
  kRightFoot,
  kBottomRight
};

inline OutlineArc ArcOfCaller(const void* caller) {
  const std::pair<uintptr_t, OutlineArc> arcs[] = {
      {edge155::rva::EdgeTabStyleViews_GetPath_leftFoot_return,
       OutlineArc::kLeftFoot},
      {edge155::rva::EdgeTabStyleViews_GetPath_topLeftArc_return,
       OutlineArc::kTopLeft},
      {edge155::rva::EdgeTabStyleViews_GetPath_topRightArc_return,
       OutlineArc::kTopRight},
      {edge155::rva::EdgeTabStyleViews_GetPath_rightFoot_return,
       OutlineArc::kRightFoot},
      {edge155::rva::EdgeTabStyleViews_GetPath_bottomRightArc_return,
       OutlineArc::kBottomRight}};
  for (const auto& [rva, arc] : arcs) {
    if (caller == edge::At(rva)) {
      return arc;
    }
  }
  return OutlineArc::kNone;
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
// The length of a chamfer's two legs for a corner of `radius`: the corner's own
// radius plus the cut, the same on a horizontal tab (arc plus h_line_to) and a
// vertical one (h_add_rrect).
inline float ChamferLeg(float radius) {
  return radius * (1 + kTrailingCutRatio);
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
  const OutlineArc corner =
      g_outlinedTab ? ArcOfCaller(_ReturnAddress()) : OutlineArc::kNone;
  const bool isHandled =
      corner != OutlineArc::kNone &&
      (!edge::IsUiRightToLeft() || corner == OutlineArc::kLeftFoot ||
       corner == OutlineArc::kRightFoot);
  if (!isHandled) {
    return o_arc_to(path, radii, rotation, arcSize, direction, end);
  }
  const PointF radius = UnpackPoint(radii);
  g_cornerRadius = radius.x;
  PointF target = UnpackPoint(end);
  switch (corner) {
    case OutlineArc::kLeftFoot:
      target.y += radius.y;
      break;
    case OutlineArc::kTopLeft:
      target.x -= radius.x;
      break;
    case OutlineArc::kTopRight:
      target.y += TrailingCut();
      break;
    case OutlineArc::kRightFoot:
      target.x -= radius.x;
      break;
    case OutlineArc::kBottomRight:
      target.x += radius.x;
      break;
    case OutlineArc::kNone:
      break;
  }
  return o_arc_to(path, PackPoint(PointF{}), rotation, arcSize, direction,
                  PackPoint(target));
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
  PointF target = UnpackPoint(point);
  target.x -= TrailingCut();
  return o_line_to(path, PackPoint(target));
}

// How a vertical tab's outline, an SkRRect from h_set_rect_xy, is turned into a
// path: SkPath::addRRect(this, const SkRRect&, direction, startIndex).
using AddRRect_t = void*(__fastcall*)(void*, const void*, int, unsigned);
inline constinit AddRRect_t o_add_rrect = nullptr;
using MoveTo_t = void*(__fastcall*)(void*, uint64_t);

// An SkRRect's layout: its rect (left, top, right, bottom), then each corner's
// radii in Skia's order (tab_edge::Corner).
inline constexpr uintptr_t kRRectLeft = 0x0;
inline constexpr uintptr_t kRRectTop = 0x4;
inline constexpr uintptr_t kRRectRight = 0x8;
inline constexpr uintptr_t kRRectBottom = 0xc;
inline constexpr uintptr_t kRRectRadii = 0x10;

// The vertical tab's outline: the call in GetPath that adds the rounded
// rectangle becomes a polygon with the same bounds, whose trailing top corner,
// the one h_set_rect_xy left round, is cut 45 degrees as a horizontal tab's is
// (TrailingCut). A tab with a split neighbour on its trailing side had no round
// corner there, so it stays a rectangle. Every other caller is Skia's own use,
// unchanged.
inline void* __fastcall h_add_rrect(void* path, const void* roundedRect,
                                    int direction, unsigned startIndex) {
  const bool isTabOutline =
      g_outlinedTab &&
      _ReturnAddress() ==
          edge::At(edge155::rva::EdgeTabStyleViews_GetPath_addRRect_return);
  if (!isTabOutline) {
    return o_add_rrect(path, roundedRect, direction, startIndex);
  }
  const float left = edge::Field<float>(roundedRect, kRRectLeft);
  const float top = edge::Field<float>(roundedRect, kRRectTop);
  const float right = edge::Field<float>(roundedRect, kRRectRight);
  const float bottom = edge::Field<float>(roundedRect, kRRectBottom);
  const bool isLeftToRight = !edge::IsUiRightToLeft();
  const tab_edge::Corner trailingTop = isLeftToRight
                                           ? tab_edge::Corner::kUpperRight
                                           : tab_edge::Corner::kUpperLeft;
  const float radius = edge::Field<float>(
      roundedRect,
      kRRectRadii + static_cast<uintptr_t>(trailingTop) * sizeof(PointF));
  const float leg = ChamferLeg(radius);
  const float trailingX = isLeftToRight ? right : left;
  const float leadingX = isLeftToRight ? left : right;
  const float legX = isLeftToRight ? -leg : leg;
  const auto point = [](float x, float y) {
    return PackPoint(PointF{x, y});
  };
  const auto moveTo =
      reinterpret_cast<MoveTo_t>(edge::At(edge155::rva::SkPath_moveTo));
  moveTo(path, point(leadingX, top));
  o_line_to(path, point(trailingX + legX, top));
  o_line_to(path, point(trailingX, top + leg));
  o_line_to(path, point(trailingX, bottom));
  o_line_to(path, point(leadingX, bottom));
  return o_line_to(path, point(leadingX, top));
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
