// tab_shape.h — the tab's outline: the Cyberpunk kit's plate, square with 45
// degree cuts on its top trailing and bottom leading corners (tab_edge.h has the
// sketch); a horizontal tab loses its flared feet. Edge outlines a tab (fill, hover, press) in
// EdgeTabStyleViews::GetPath, as one rounded rectangle for a vertical tab and
// four arcs for a horizontal one; changing the outline shapes the tab in
// whatever colour Edge paints it, so nothing is painted over the fill except a
// split group's (SplitTabView::OnPaint). The same outline, through GetPath,
// is what the horizontal strip's drop shadow follows. In a right-to-left UI the
// whole outline mirrors: a vertical tab's by tab_edge::LeadingSide, a horizontal
// one's by Edge, which draws it left to right on a flipped canvas. UI thread
// only.
#pragma once
#include <windows.h>
#include <intrin.h>
#include <bit>
#include <utility>
#include <vector>
#include "edge/edge.h"
#include "edge/geom.h"
#include "layout/state.h"
#include "chrome/tabs/tab_edge.h"

namespace edgeplus::tab_shape {

// The Cyberpunk skin's switch: this outline, and the red selected tab
// (cyberpunk_skin.h) that hooks.h fills it with.
inline constinit bool g_isCyberpunkSkinOn = false;

// The tab GetPath is outlining right now, so the Skia calls it makes can be told
// from every other.
inline constinit void* g_outlinedTab = nullptr;
// Whether that tab's plate is cut (tab_edge::IsPlateCut); square when not. Every
// way GetPath outlines it follows this.
inline constinit bool g_isPlateCut = true;

inline bool IsPlateCutFor(void* tabView) {
  void* tabObject = edge::TabObjectOfView(tabView);
  return tab_edge::IsPlateCut(edge::IsTabObjectInSplit(tabObject),
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
  g_outlinedTab =
      g_isCyberpunkSkinOn ? edge::TabViewOfStyleViews(styleViews) : nullptr;
  g_isPlateCut = !g_outlinedTab || IsPlateCutFor(g_outlinedTab);
  void* result = o_get_path(styleViews, outPath, pathType, scale, isUnscaled);
  g_outlinedTab = nullptr;
  g_isOutliningShadow = false;
  return result;
}

// EdgeTabStyleViews::IsLeftSplitTab / IsRightSplitTab(Tab*): whether GetPath
// outlines a tab as its split's first member (no trailing arc, a square right
// side) or a later one (no leading foot). Neither while it outlines the
// shadowed tab; and while we shape the outline no tab takes the first member's
// square-sided branch, since g_isPlateCut squares or cuts every member.
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
  edge::SetRoundedRectRadii(roundedRect, skRect,
                            tab_edge::RadiiFor(leading, radiusX, g_isPlateCut));
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

// How far each cut reaches past its corner's radius, as a share of it: enough
// to read as a chamfer, short of the favicon and the close button.
inline constexpr float kCutRatio = 0.75f;
// How far a cut reaches along each of its sides, on a horizontal tab (arcs plus
// h_line_to) and a vertical one (h_add_rrect) alike.
inline float ChamferLeg(float radius) {
  return radius * (1 + kCutRatio);
}

using LineTo_t = void*(__fastcall*)(void*, uint64_t);
inline constinit LineTo_t o_line_to = nullptr;

// Set by the top-left arc, for the top edge after it.
inline constinit float g_cornerRadius = 0;
// Set by the top edge (h_line_to): how far back from the corner it stopped, so
// the top-right arc ends the same distance down the trailing side and the cut
// between them is at 45 degrees.
inline constinit float g_topCutLeg = 0;

// SkPath::arcTo, for the calls in GetPath that round a tab's outline (both of a
// horizontal tab's, and a vertical tab's pathTypes 0 and 1). With no radius an
// arc is a straight line to its end, so every corner comes out square: the
// flared feet end level with the tab's bottom and the bottom-right corner meets
// the side. The plate's two cuts are where a cut plate leaves it:
// - the bottom leading cut: the left foot first runs along the bottom past the
//   side, then rises at 45 degrees to the side;
// - the top trailing cut: the top edge stops short of the corner (h_line_to) and
//   the top-right arc drops to the side from there.
// Every point is relative to where Edge's own arc ends, so the same moves fit a
// horizontal tab's flared foot and a vertical tab's rounded corner. A
// right-to-left UI needs nothing more: GetPath builds the same outline left to
// right whatever the direction (the same calls from the same addresses) and Edge
// flips the canvas it paints on, so the cuts show mirrored.
inline void* __fastcall h_arc_to(void* path, uint64_t radii, float rotation,
                                 int arcSize, uint8_t direction, uint64_t end) {
  const OutlineArc corner =
      g_outlinedTab ? ArcOfCaller(_ReturnAddress()) : OutlineArc::kNone;
  const bool isOutlineArc = corner != OutlineArc::kNone;
  if (!isOutlineArc) {
    return o_arc_to(path, radii, rotation, arcSize, direction, end);
  }
  const PointF radius = UnpackPoint(radii);
  PointF target = UnpackPoint(end);
  switch (corner) {
    case OutlineArc::kLeftFoot:
      // The arc ends on the left side, one radius above the bottom.
      target.y += radius.y;
      if (g_isPlateCut) {
        const float leg = ChamferLeg(radius.x);
        o_line_to(path, PackPoint(PointF{target.x + leg, target.y}));
        target.y -= leg;
      }
      break;
    case OutlineArc::kTopLeft:
      g_cornerRadius = radius.x;
      target.x -= radius.x;
      break;
    case OutlineArc::kTopRight:
      // The arc ends on the right side, one radius below the top, and the top
      // edge stopped g_topCutLeg short of the corner.
      target.y += g_topCutLeg - radius.y;
      break;
    case OutlineArc::kRightFoot:
      // The foot flares one radius out past the right side.
      target.x -= radius.x;
      break;
    case OutlineArc::kBottomRight:
      // The arc ends one radius in from the right side.
      target.x += radius.x;
      break;
    case OutlineArc::kNone:
      break;
  }
  return o_arc_to(path, PackPoint(PointF{}), rotation, arcSize, direction,
                  PackPoint(target));
}

// SkPath::lineTo, for the call in GetPath that draws the top edge: it runs on to
// the top-right corner instead of stopping where its arc would start, or to a
// cut's length short of it on a cut plate.
inline void* __fastcall h_line_to(void* path, uint64_t point) {
  const bool isTopEdge =
      g_outlinedTab &&
      _ReturnAddress() ==
          edge::At(edge155::rva::EdgeTabStyleViews_GetPath_topEdge_return);
  if (!isTopEdge) {
    return o_line_to(path, point);
  }
  g_topCutLeg = g_isPlateCut ? ChamferLeg(g_cornerRadius) : 0;
  PointF target = UnpackPoint(point);
  target.x += g_cornerRadius - g_topCutLeg;
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

// The vertical tab's rounded rectangle becomes the plate with the same bounds,
// cut at the two corners h_set_rect_xy left round. An uncut plate has no radius
// there, so stays a rectangle.
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
  const tab_edge::Side leading = tab_edge::LeadingSide(edge::IsUiRightToLeft());
  const tab_edge::Corner trailingTop = tab_edge::TrailingTopCorner(leading);
  const float radius = edge::Field<float>(
      roundedRect,
      kRRectRadii + static_cast<uintptr_t>(trailingTop) * sizeof(PointF));
  const tab_edge::CutOutline outline = tab_edge::CutOutlineOf(
      left, top, right, bottom, ChamferLeg(radius), leading);
  const auto moveTo =
      reinterpret_cast<MoveTo_t>(edge::At(edge155::rva::SkPath_moveTo));
  moveTo(path, PackPoint(outline.points[0]));
  void* result = path;
  for (size_t i = 1; i < tab_edge::kCutOutlinePointCount; i++) {
    result = o_line_to(path, PackPoint(outline.points[i]));
  }
  return result;
}

using DrawRoundRect_t = void(__fastcall*)(void*, const Rect*, int, const void*);
inline constinit DrawRoundRect_t o_draw_round_rect = nullptr;

// Canvas::DrawRoundRect, for the one call that paints a split group's rounded
// fill (SplitTabView::OnPaint): the same paint then goes over the leading
// corners it rounded.
inline void __fastcall h_draw_round_rect(void* canvas, const Rect* rect,
                                         int radius, const void* flags) {
  const bool isSplitGroupFill =
      g_isCyberpunkSkinOn &&
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

}  // namespace edgeplus::tab_shape
