// tab_frame.h — the Cyberpunk skin's 2px frame on the selected tab, in the
// kit's accent (cyberpunk_skin::SelectedTabFor); the kit's tab control leaves
// the other tabs unframed. Nothing here paints: Edge already strokes the active
// tab's outline in EdgeTabStyleViews::PaintBackgroundStroke, with its own path
// (GetPath's border, which tab_shape.h squares like the fill) and its own paint
// flags, but only while the strip asks for strokes, which a vertical strip
// never does. These hooks hand that stroke a thickness and a colour. UI thread
// only.
#pragma once
#include "edge/edge.h"
#include "theme/cyberpunk_skin.h"
#include "theme/skin_tone.h"
#include "chrome/tabs/tab_shape.h"

namespace edgeplus::tab_frame {

// The stroke width h_get_stroke_thickness hands out, set only for the length
// of one PaintBackgroundStroke the skin frames; 0 the rest of the time.
inline constinit int g_frameThickness = 0;

using PaintBackgroundStroke_t = void(__fastcall*)(void*, void*, uint32_t);
inline constinit PaintBackgroundStroke_t o_paint_background_stroke = nullptr;

inline void __fastcall h_paint_background_stroke(void* styleViews, void* canvas,
                                                 uint32_t strokeColor) {
  void* const tab = edge::TabObjectOfStyleViews(styleViews);
  const bool isFramed = tab_shape::g_isCyberpunkSkinOn && (tab != nullptr) &&
                        edge::IsTabObjectActive(tab);
  if (!isFramed) {
    o_paint_background_stroke(styleViews, canvas, strokeColor);
    return;
  }
  g_frameThickness = cyberpunk_skin::kFrameThicknessDip;
  o_paint_background_stroke(
      styleViews, canvas,
      cyberpunk_skin::SelectedTabFor(skin_tone::Current()).frame);
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

}  // namespace edgeplus::tab_frame
