// tab_shape.h — the tab's outline: a straight leading edge and a 45 degree cut
// on the trailing bottom corner. Edge outlines a tab (fill, hover, press) in
// EdgeTabStyleViews::GetPath, as one rounded rectangle for a vertical tab and
// four arcs for a horizontal one; changing the outline shapes the tab in
// whatever colour Edge paints it, so nothing is painted over the fill except a
// split group's (SplitTabView::OnPaint). The same outline, through GetPath,
// is what the horizontal strip's drop shadow follows. UI thread only.
#pragma once
#include <windows.h>
#include <intrin.h>
#include <bit>
#include <utility>
#include <vector>
#include "edge.h"
#include "geom.h"
#include "state.h"
#include "tab_edge.h"

namespace fixedge::tab_shape {

inline constinit bool g_isSquared = false;

// The tab GetPath is outlining right now, so the Skia calls it makes can be told
// from every other.
inline constinit void* g_outlinedTab = nullptr;
// Whether that tab's trailing bottom corner is cut (tab_edge::IsTrailingCornerCut);
// square when not. Every way GetPath outlines it follows this.
inline constinit bool g_isTrailingCut = true;

inline bool IsTrailingCutFor(void* tabView) {
  void* tabObject = edge::TabObjectOfView(tabView);
  return tab_edge::IsTrailingCornerCut(edge::IsTabObjectInSplit(tabObject),
                                       edge::IsTheActiveTabObject(tabObject));
}

using GetPath_t = void*(__fastcall*)(void*, void*, int, float, const int*);
inline constinit GetPath_t o_get_path = nullptr;

// Whether GetPath is outlining the active tab for the horizontal strip's drop
// shadow, which then outlines it as a tab of its own (h_is_left_split_tab,
// h_is_right_split_tab).
inline constinit bool g_isOutliningShadow = false;

// Also the drop shadow (EdgeTabStrip::UpdateDropShadow). Edge shadows the
// active tab's split as one outline: the active tab's open outline, the seam
// side left off, extended by its split neighbour's (TabStrip::GetTabsInSplit,
// one neighbour only), which in a split grown past two draws a stray diagonal
// across both tabs. Only the focused pane's tab paints as active, so only it is
// shadowed: the neighbour's outline comes back empty and the active tab's is
// a whole tab's, which Edge closes.
inline void* __fastcall h_get_path(void* styleViews, void* outPath,
                                   int pathType, float scale,
                                   const int* isUnscaled) {
  const void* caller = _ReturnAddress();
  const bool isSplitNeighbourShadow =
      SplitActive() &&
      caller ==
          edge::At(edge155::rva::
                       EdgeTabStrip_UpdateDropShadow_splitMemberPath_return);
  if (isSplitNeighbourShadow) {
    return edge::ConstructEmptyPath(outPath);
  }
  g_isOutliningShadow =
      SplitActive() &&
      caller ==
          edge::At(
              edge155::rva::EdgeTabStrip_UpdateDropShadow_activeTabPath_return);
  g_outlinedTab = g_isSquared ? edge::TabViewOfStyleViews(styleViews) : nullptr;
  g_isTrailingCut = !g_outlinedTab || IsTrailingCutFor(g_outlinedTab);
  void* result = o_get_path(styleViews, outPath, pathType, scale, isUnscaled);
  g_outlinedTab = nullptr;
  g_isOutliningShadow = false;
  return result;
}

// EdgeTabStyleViews::IsLeftSplitTab / IsRightSplitTab(Tab*): whether GetPath
// outlines a tab as its split's first member (no trailing arc, a square right
// side) or a later one (no leading foot). Neither while it outlines the
// shadowed tab; and while we shape the outline no tab takes the first member's
// square-sided branch, since g_isTrailingCut squares or cuts every member.
using IsSplitSide_t = bool(__fastcall*)(void*);
inline constinit IsSplitSide_t o_is_left_split_tab = nullptr;
inline constinit IsSplitSide_t o_is_right_split_tab = nullptr;

inline bool __fastcall h_is_left_split_tab(void* tabObject) {
  const bool isOwnOutline = g_isOutliningShadow || g_outlinedTab;
  if (isOwnOutline) {
    return false;
  }
  return o_is_left_split_tab(tabObject);
}

inline bool __fastcall h_is_right_split_tab(void* tabObject) {
  if (g_isOutliningShadow) {
    return false;
  }
  return o_is_right_split_tab(tabObject);
}

using SetRectXY_t = void(__fastcall*)(void*, const void*, float, float);
inline constinit SetRectXY_t o_set_rect_xy = nullptr;

// SkRRect::setRectXY, for the one call that builds a vertical tab's outline: the
// radius is Edge's, the corners are ours.
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
  edge::SetRoundedRectRadii(
      roundedRect, skRect,
      tab_edge::RadiiFor(leading, radiusX, g_isTrailingCut));
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

// How far the trailing bottom corner's cut reaches past the corner's radius, as
// a share of it: enough to read as a chamfer, short of the close button.
inline constexpr float kTrailingCutRatio = 0.75f;
// Set by the arcs before the right side.
inline constinit float g_cornerRadius = 0;
inline float TrailingCut() {
  return g_cornerRadius * kTrailingCutRatio;
}
// The same on a horizontal tab (arc plus h_line_to) and a vertical one
// (h_add_rrect).
inline float ChamferLeg(float radius) {
  return radius * (1 + kTrailingCutRatio);
}

// SkPath::arcTo, for the calls in GetPath that round a horizontal tab's outline.
// With no radius an arc is a straight line to its end, so the flared feet end
// level with the tab's bottom, the top corners are square and the right foot
// (or a vertical tab's bottom-right arc) becomes the cut, from where h_line_to
// stops the right side. A right-to-left UI keeps Edge's corners: its leading
// corner is built from different calls.
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
      // The top edge already reached the corner (h_line_to): this is the
      // straight start of the drop down the trailing side.
      break;
    case OutlineArc::kRightFoot:
      // The foot flares one radius out past the right side.
      target.x -= g_isTrailingCut ? radius.x + ChamferLeg(radius.x) : radius.x;
      break;
    case OutlineArc::kBottomRight:
      // The arc ends one radius in from the right side.
      target.x += g_isTrailingCut ? -TrailingCut() : radius.x;
      break;
    case OutlineArc::kNone:
      break;
  }
  return o_arc_to(path, PackPoint(PointF{}), rotation, arcSize, direction,
                  PackPoint(target));
}

using LineTo_t = void*(__fastcall*)(void*, uint64_t);
inline constinit LineTo_t o_line_to = nullptr;

// SkPath::lineTo, for two calls in GetPath: the top edge, which runs on to the
// square top-right corner instead of stopping where its arc would start, and
// the right side, which stops a cut's length above where the bottom corner's
// arc would start when that corner is cut.
inline void* __fastcall h_line_to(void* path, uint64_t point) {
  const void* caller = _ReturnAddress();
  const bool isShaped = g_outlinedTab && !edge::IsUiRightToLeft();
  const bool isTopEdge =
      isShaped &&
      caller ==
          edge::At(edge155::rva::EdgeTabStyleViews_GetPath_topEdge_return);
  const bool isCutRightEdge =
      isShaped && g_isTrailingCut &&
      caller ==
          edge::At(edge155::rva::EdgeTabStyleViews_GetPath_rightEdge_return);
  PointF target = UnpackPoint(point);
  if (isTopEdge) {
    target.x += g_cornerRadius;
  } else if (isCutRightEdge) {
    target.y -= TrailingCut();
  } else {
    return o_line_to(path, point);
  }
  return o_line_to(path, PackPoint(target));
}

// SkPath::addRRect(this, const SkRRect&, direction, startIndex).
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

// The vertical tab's rounded rectangle becomes a polygon with the same bounds,
// its trailing bottom corner (the one h_set_rect_xy left round) cut. A tab with a
// split neighbour on its trailing side has no radius there, so stays a
// rectangle.
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
  const tab_edge::Corner trailingBottom = isLeftToRight
                                              ? tab_edge::Corner::kLowerRight
                                              : tab_edge::Corner::kLowerLeft;
  const float radius = edge::Field<float>(
      roundedRect,
      kRRectRadii + static_cast<uintptr_t>(trailingBottom) * sizeof(PointF));
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
  o_line_to(path, point(trailingX, top));
  o_line_to(path, point(trailingX, bottom - leg));
  o_line_to(path, point(trailingX + legX, bottom));
  o_line_to(path, point(leadingX, bottom));
  return o_line_to(path, point(leadingX, top));
}

using DrawRoundRect_t = void(__fastcall*)(void*, const Rect*, int, const void*);
inline constinit DrawRoundRect_t o_draw_round_rect = nullptr;

// Canvas::DrawRoundRect, for the one call that paints a split group's rounded
// fill (SplitTabView::OnPaint): the same paint then goes over the leading
// corners it rounded.
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
