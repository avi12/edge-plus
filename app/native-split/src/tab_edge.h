// tab_edge.h — where the selected vertical tab's spine and squared corners go.
// One concern: the pure geometry of the pieces spine.h lays over the tab's
// leading edge (the left one, the right one when Edge's UI runs right to left):
// the stripe the full height of the row, and a square of the selected fill over
// each of the pill's two rounded corners on that side, just inside the stripe;
// and the stripe's colour for the theme's tone. No Edge, no Win32.
#pragma once
#include "geom.h"
#include "theme_tone.h"

namespace fixedge::tab_edge {

// The stripe's neon magenta on a dark rail, and a deep magenta on a light one,
// where the neon would read at only 2.5:1.
inline constexpr theme_tone::Argb kStripeOnDark = 0xffff2bd6;
inline constexpr theme_tone::Argb kStripeOnLight = 0xffc8008f;

// A theme that says nothing of its tone gets the neon, as before there were two.
constexpr theme_tone::Argb StripeColorFor(theme_tone::Tone tone) {
  return tone == theme_tone::Tone::kLight ? kStripeOnLight : kStripeOnDark;
}

// The side of the tab the spine marks: its leading edge, where the favicon is.
enum class Side {
  kLeft,
  kRight,
};

constexpr Side LeadingSide(bool isRightToLeft) {
  return isRightToLeft ? Side::kRight : Side::kLeft;
}

// The stripe and the two corner squares, in the tab's own coordinate space.
struct Pieces {
  Rect stripe;
  Rect topCorner;
  Rect bottomCorner;
};

// `tab` is the tab row; `stripeWidth` and `cornerSize` are in the same units.
// A corner square is `cornerSize` tall and fills what the stripe leaves of
// `cornerSize` across, so stripe and square together cover the curve.
constexpr Pieces PiecesFor(Rect tab, Side side, int stripeWidth,
                           int cornerSize) {
  const int cornerWidth = cornerSize - stripeWidth;
  const bool isOnLeft = side == Side::kLeft;
  const int stripeX = isOnLeft ? tab.x : tab.x + tab.width - stripeWidth;
  const int cornerX =
      isOnLeft ? tab.x + stripeWidth : tab.x + tab.width - cornerSize;
  const int bottomY = tab.y + tab.height - cornerSize;
  return Pieces{
      Rect{stripeX, tab.y, stripeWidth, tab.height},
      Rect{cornerX, tab.y, cornerWidth, cornerSize},
      Rect{cornerX, bottomY, cornerWidth, cornerSize},
  };
}

}  // namespace fixedge::tab_edge
