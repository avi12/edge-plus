// menu_border_insets.h — pure arithmetic for the Cyberpunk skin's menu border
// (no Edge, no Win32). A menu's border is views::BubbleBorder's: a 1px line
// painted one pixel inside the insets that the border reports. On the sides and
// the bottom those insets hold the shadow; on the top the shadow's margin is
// swallowed and the insets are 0, so the line lands one pixel above the
// menu's window and is clipped away.
#pragma once

namespace edgeplus::menu_border_insets {

// gfx::Insets as Edge lays it out in memory.
struct Insets {
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;

  friend constexpr bool operator==(const Insets&, const Insets&) = default;
};

// The border line's own thickness: the room the top must give it.
inline constexpr int kBorderThickness = 1;

// The menu Edge has open, if any.
enum class OpenMenu {
  kNone,
  kContext,
  kHangingFromButton,
};

// views::BubbleBorder::Arrow, read from the border at +0x10: the values of a
// bubble that hangs below its anchor, with the anchor against the bubble's top
// edge. Measured: the profile, favorites and add-favorite bubbles are 1, the
// site-info bubble is 0; the web-capture toolbar, which floats, is 10, and a
// menu's border is 16 whatever hangs it.
enum class BubbleArrow : int {
  kTopLeft = 0,
  kTopRight = 1,
  kTopCenter = 8,
};

constexpr bool IsHangingBelowAnchor(int arrow) {
  return arrow == static_cast<int>(BubbleArrow::kTopLeft) ||
         arrow == static_cast<int>(BubbleArrow::kTopRight) ||
         arrow == static_cast<int>(BubbleArrow::kTopCenter);
}

// Whether the border is to carry its line on the top edge as on the others.
// The top meets the toolbar for a menu hung from a button and for a bubble
// hung from its anchor, which keep none. A menu's own kind decides while one is
// open (its border does not say), a bubble's arrow decides otherwise.
constexpr bool WantsTopLine(OpenMenu openMenu, int arrow) {
  const bool isMenuOpen = openMenu != OpenMenu::kNone;
  if (isMenuOpen) {
    return openMenu == OpenMenu::kContext;
  }
  return !IsHangingBelowAnchor(arrow);
}

// A side with no room at all is given the line's; a side that has room keeps
// Edge's answer.
constexpr Insets WithRoomForTopLine(Insets insets) {
  const bool isTopLineClipped = insets.top < kBorderThickness;
  if (isTopLineClipped) {
    insets.top = kBorderThickness;
  }
  return insets;
}

}  // namespace edgeplus::menu_border_insets
