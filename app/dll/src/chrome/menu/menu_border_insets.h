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
