// omnibox_frame.h — the Cyberpunk skin frames the omnibox in the kit's idle
// line colour (cyberpunk_skin::IdleLineFor), the way the kit frames a text
// field. Nothing here paints: LocationBarView::RefreshBackground builds the
// omnibox's background with EdgeCreateRoundRectBackground, which already draws
// a 1px border in it when handed a stroke colour (only Edge's
// kOmniboxLocationBarStrokeAndHoverRefresh feature does); the skin hands it
// one. The background is rebuilt on focus, hover and theme changes, so a
// switch shows on the next of those. UI thread only.
#pragma once
#include <intrin.h>
#include "edge/edge.h"
#include "theme/cyberpunk_skin.h"
#include "theme/skin_tone.h"
#include "chrome/tabs/tab_shape.h"

namespace edgeplus::omnibox_frame {

using CreateRoundRectBackground_t = void*(__fastcall*)(void*, void*, uint32_t,
                                                       uint32_t, int, bool,
                                                       bool);
inline constinit CreateRoundRectBackground_t o_create_background = nullptr;

inline void* __fastcall h_create_background(void* locationBar, void* out,
                                            uint32_t background,
                                            uint32_t stroke, int blendMode,
                                            bool isAntialiased,
                                            bool shouldBorderScale) {
  const bool isOmniboxBackground =
      tab_shape::g_isCyberpunkSkinOn &&
      _ReturnAddress() ==
          edge::At(edge155::rva::
                       LocationBarView_RefreshBackground_background_return);
  if (isOmniboxBackground) {
    stroke = cyberpunk_skin::IdleLineFor(skin_tone::Current());
  }
  return o_create_background(locationBar, out, background, stroke, blendMode,
                             isAntialiased, shouldBorderScale);
}

}  // namespace edgeplus::omnibox_frame
