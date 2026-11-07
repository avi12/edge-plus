// tab_frame.h — the Cyberpunk skin's 2px tab frames: the selected tab framed in
// the kit's frame colour, every other tab in its idle line colour
// (cyberpunk_skin::FramesFor). Nothing here paints: Edge already strokes the
// active tab's outline in EdgeTabStyleViews::PaintBackgroundStroke, with its own
// path (GetPath's border, which tab_shape.h cuts like the fill) and its own
// paint flags, but only while the strip asks for strokes, which a vertical strip
// never does. These hooks hand that stroke a thickness, a colour, and, for an
// idle tab, the one "is active" answer that lets it run. UI thread only.
#pragma once
#include <optional>
#include "edge/edge.h"
#include "theme/cyberpunk_skin.h"
#include "theme/theme_tone.h"
#include "chrome/tabs/tab_shape.h"

namespace edgeplus::tab_frame {

// The stroke width h_get_stroke_thickness hands out, set only for the length
// of one PaintBackgroundStroke the skin frames; 0 the rest of the time.
inline constinit int g_frameThickness = 0;
// A selected row of the vertical list has its border path inset, so its whole
// stroke shows; an idle row's runs along the row's bounds, which clip the outer
// half away (seen at runtime, docs/re/cyberpunk-skin.md). Twice the width
// leaves the same frame inside. A horizontal tab's stroke is never clipped.
inline constexpr int kIdleStrokeWidthFactor = 2;
// Set just before an idle tab's PaintBackgroundStroke: its first question,
// whether the tab is active, is answered yes once (h_is_active), so it strokes.
inline constinit bool g_shouldAnswerActiveOnce = false;

// The tone the skin chose the selected tab's fill for, read back off that fill
// (hooks.h h_getuicolor answers the active tab's background with it).
inline theme_tone::Tone ToneOfTab(void* tabView) {
  const std::optional<uint32_t> selectedFill = edge::ViewColor(
      tabView, edge155::off::UICOLOR_active_tab_background_focused);
  if (!selectedFill) {
    return theme_tone::Tone::kUnknown;
  }
  return cyberpunk_skin::ToneOfSelectedFill(*selectedFill);
}

using PaintBackgroundStroke_t = void(__fastcall*)(void*, void*, uint32_t);
inline constinit PaintBackgroundStroke_t o_paint_background_stroke = nullptr;

inline void __fastcall h_paint_background_stroke(void* styleViews, void* canvas,
                                                 uint32_t strokeColor) {
  void* const tab = edge::TabObjectOfStyleViews(styleViews);
  const bool isFramed = tab_shape::g_isCyberpunkSkinOn && tab;
  if (!isFramed) {
    o_paint_background_stroke(styleViews, canvas, strokeColor);
    return;
  }
  void* const tabView = edge::TabViewOfStyleViews(styleViews);
  const bool isSelected = edge::IsTabObjectActive(tab);
  const cyberpunk_skin::TabFrames frames =
      cyberpunk_skin::FramesFor(ToneOfTab(tabView));
  const bool isClippedIdleRow = !isSelected && edge::IsVerticalTabView(tabView);
  g_frameThickness = isClippedIdleRow ? cyberpunk_skin::kFrameThicknessDip *
                                            kIdleStrokeWidthFactor
                                      : cyberpunk_skin::kFrameThicknessDip;
  g_shouldAnswerActiveOnce = !isSelected;
  o_paint_background_stroke(styleViews, canvas,
                            isSelected ? frames.selected : frames.idle);
  // Cleared whatever the stroke asked: a flag left set would answer some later,
  // unrelated question.
  g_shouldAnswerActiveOnce = false;
  g_frameThickness = 0;
}

// int TabStrip::GetStrokeThickness(this): the frame's thickness while one is
// stroked; Edge's own answer (none for a vertical strip) everywhere else.
using GetStrokeThickness_t = int(__fastcall*)(void*);
inline constinit GetStrokeThickness_t o_get_stroke_thickness = nullptr;

inline int __fastcall h_get_stroke_thickness(void* controller) {
  const bool isFraming = g_frameThickness != 0;
  if (isFraming) {
    return g_frameThickness;
  }
  return o_get_stroke_thickness(controller);
}

// bool EdgeTab::IsActive(this): PaintBackgroundStroke's gate. Answered yes once
// for an idle tab being framed; its GetPath, which also asks, then hears the
// truth, so the idle outline keeps an idle tab's shape.
using IsActive_t = bool(__fastcall*)(void*);
inline constinit IsActive_t o_is_active = nullptr;

inline bool __fastcall h_is_active(void* tab) {
  if (g_shouldAnswerActiveOnce) {
    g_shouldAnswerActiveOnce = false;
    return true;
  }
  return o_is_active(tab);
}

}  // namespace edgeplus::tab_frame
