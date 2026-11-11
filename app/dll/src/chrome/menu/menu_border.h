// menu_border.h — the Cyberpunk skin's red menu border, on the top edge too.
// views::BubbleBorder::GetInsets answers 0 for the top of a menu, so the 1px
// border line (painted one pixel inside the insets) falls outside the menu's
// window there and is clipped; the sides and bottom carry the shadow's margin
// and show it. A menu that hangs from a toolbar button meets the toolbar at its
// top and keeps none. While a context menu is open and the skin is on, the top is given the
// line's pixel (menu_border_insets.h); otherwise Edge's answer stands.
// docs/re/cyberpunk-skin.md, "Menu border". UI thread only.
#pragma once
#include "chrome/menu/menu_border_insets.h"
#include "chrome/tabs/tab_shape.h"
#include "edge/edge.h"

namespace edgeplus::menu_border {

using GetInsets_t = void*(__fastcall*)(void*, void*);
inline constinit GetInsets_t o_get_insets = nullptr;

// BubbleBorder::GetInsets(this, gfx::Insets* out) returns `out`.
inline void* __fastcall h_get_insets(void* border, void* out) {
  void* const result = o_get_insets(border, out);
  const bool isContextMenuBorder =
      tab_shape::g_isCyberpunkSkinOn && edge::IsContextMenuOpen();
  if (isContextMenuBorder) {
    auto* insets = static_cast<menu_border_insets::Insets*>(out);
    *insets = menu_border_insets::WithRoomForTopLine(*insets);
  }
  return result;
}

}  // namespace edgeplus::menu_border
