// reserve.h — the width an open Gemini panel takes from the page. One concern:
// a window's reserved width, written by gemini.h when its panel opens, closes
// or is resized, and read by the MultiContentsView layout detour (hooks.h) and
// the split's content box (geometry.h), so the page and its panes are laid out
// beside the panel instead of under it.
#pragma once
#include <map>

namespace fixedge::reserve {

inline std::map<void*, int>& Widths() {
  static std::map<void*, int> widths;
  return widths;
}

// DIPs taken from the right of `mcv`'s content; 0 for no panel.
inline int WidthFor(void* mcv) {
  const auto entry = Widths().find(mcv);
  return entry == Widths().end() ? 0 : entry->second;
}

inline void Set(void* mcv, int width) {
  if (width > 0) {
    Widths()[mcv] = width;
  } else {
    Widths().erase(mcv);
  }
}

}  // namespace fixedge::reserve
