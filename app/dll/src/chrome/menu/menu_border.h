// menu_border.h — the Cyberpunk skin's red border line, on the top edge too.
// views::BubbleBorder::GetInsets answers 0 for the top of a menu or bubble, so
// the 1px border line (painted one pixel inside the insets) falls outside the
// window there and is clipped; the sides and bottom carry the shadow's margin
// and show it. With the skin on, the top is given the line's pixel
// (menu_border_insets.h) like the other sides, so the line always has room,
// except where the top meets the toolbar: a menu hung from a button and a
// bubble hung from its anchor keep Edge's answer. UI thread only.
// docs/re/cyberpunk-skin.md, "Menu border".
#pragma once
#include "chrome/menu/menu_border_insets.h"
#include "chrome/tabs/tab_shape.h"
#include "edge/edge.h"

namespace edgeplus::menu_border {

using GetInsets_t = void*(__fastcall*)(void*, void*);
inline constinit GetInsets_t o_get_insets = nullptr;

inline menu_border_insets::OpenMenu OpenMenuKind() {
  if (!edge::IsMenuOpen()) {
    return menu_border_insets::OpenMenu::kNone;
  }
  return edge::IsContextMenuOpen()
             ? menu_border_insets::OpenMenu::kContext
             : menu_border_insets::OpenMenu::kHangingFromButton;
}

// BubbleBorder::GetInsets(this, gfx::Insets* out) returns `out`.
inline void* __fastcall h_get_insets(void* border, void* out) {
  void* const result = o_get_insets(border, out);
  if (!tab_shape::g_isCyberpunkSkinOn) {
    return result;
  }
  const bool isTopLineWanted = menu_border_insets::WantsTopLine(
      OpenMenuKind(), edge::BubbleBorderArrow(border));
  if (isTopLineWanted) {
    auto* insets = static_cast<menu_border_insets::Insets*>(out);
    *insets = menu_border_insets::WithRoomForTopLine(*insets);
  }
  return result;
}

}  // namespace edgeplus::menu_border
