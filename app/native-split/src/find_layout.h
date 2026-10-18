// find_layout.h — where the find bar sits. One concern, pure (no Edge, no Win32;
// unit-tested in test/find_layout_test.cpp): a small card over the top right of
// the page, like Edge's own find bar, clear of the page's scroll bar.
#pragma once
#include "geom.h"

namespace fixedge::find_layout {

// DIPs: the bar's size, and its gap to the page's top and right edges.
inline constexpr int kBarWidth = 480;
inline constexpr int kBarHeight = 84;
inline constexpr int kTopGap = 6;
inline constexpr int kRightGap = 24;

// The bar's rectangle in `content`, the area the page occupies (same
// coordinates). On a page narrower than the bar it takes the width that is
// left, and on a page shorter than the bar, the height.
inline Rect BarRect(const Rect& content) {
  const int availableWidth = content.width - kRightGap;
  const int width = kBarWidth < availableWidth ? kBarWidth : availableWidth;
  const int availableHeight = content.height - kTopGap;
  const int height =
      kBarHeight < availableHeight ? kBarHeight : availableHeight;
  Rect bar;
  bar.width = width > 0 ? width : 0;
  bar.height = height > 0 ? height : 0;
  bar.x = content.x + content.width - kRightGap - bar.width;
  bar.y = content.y + kTopGap;
  return bar;
}

// Copilot's page, shown under the bar only when it needs the user (to sign in
// or accept something): as wide as the bar, as tall as fits.
inline constexpr int kChatHeight = 520;
inline constexpr int kChatGap = 4;

inline Rect ChatRect(const Rect& content) {
  const Rect bar = BarRect(content);
  Rect chat;
  chat.x = bar.x;
  chat.y = bar.y + bar.height + kChatGap;
  chat.width = bar.width;
  const int availableHeight = content.y + content.height - chat.y - kTopGap;
  const int height =
      kChatHeight < availableHeight ? kChatHeight : availableHeight;
  chat.height = height > 0 ? height : 0;
  return chat;
}

}  // namespace fixedge::find_layout
