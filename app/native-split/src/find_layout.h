// find_layout.h — where the assistant's chat page sits when it needs the user.
// One concern, pure (no Edge, no Win32; unit-tested in test/find_layout_test.cpp):
// a card at the top right of the page, just under Edge's own find bar and clear
// of the page's scroll bar, for the sign-in or consent page the enhanced find
// shows only when the assistant asks for the person.
#pragma once
#include "geom.h"

namespace fixedge::find_layout {

// DIPs: the card's size, and its gaps to the page's top (room for Edge's find
// bar) and right edges.
inline constexpr int kChatWidth = 480;
inline constexpr int kChatHeight = 520;
inline constexpr int kTopGap = 56;
inline constexpr int kBottomGap = 6;
inline constexpr int kRightGap = 24;

// The card's rectangle in `content`, the area the page occupies (same
// coordinates). On a page narrower or shorter than the card it takes what is
// left, down to nothing.
inline Rect ChatRect(const Rect& content) {
  const int availableWidth = content.width - kRightGap;
  const int width = kChatWidth < availableWidth ? kChatWidth : availableWidth;
  const int availableHeight = content.height - kTopGap - kBottomGap;
  const int height =
      kChatHeight < availableHeight ? kChatHeight : availableHeight;
  Rect chat;
  chat.width = width > 0 ? width : 0;
  chat.height = height > 0 ? height : 0;
  chat.x = content.x + content.width - kRightGap - chat.width;
  chat.y = content.y + kTopGap;
  return chat;
}

}  // namespace fixedge::find_layout
