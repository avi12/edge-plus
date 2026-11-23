// find_bar_width.h — the bar widened for a question, anchored by its right
// edge as Edge anchors it, and its own width given back after.
#pragma once
#include <windows.h>
#include <algorithm>
#include "find/find_poll.h"
#include "find/find_session.h"

namespace edgeplus::find {

// A question needs room, so with Ask on the bar is as wide again as Edge makes
// it, anchored by its right edge as Edge anchors it; the box takes the room,
// since it is the bar's flexible view.
inline constexpr int kAskWidthFactor = 2;
inline constexpr int kBarMarginPixels = 24;
inline constexpr int kBarMaxWidthPixels = 1100;  // wider than any window's bar

// The bar has a top-level window of its own: untitled and far narrower than the
// browser window.
inline HWND BarWindowOf(HWND browserWindow) {
  struct Search {
    HWND browser;
    HWND found;
  } search{browserWindow, nullptr};
  EnumThreadWindows(
      GetCurrentThreadId(),
      [](HWND candidate, LPARAM parameter) -> BOOL {
        auto* inner = reinterpret_cast<Search*>(parameter);
        RECT candidateRect{};
        RECT browserRect{};
        GetWindowRect(candidate, &candidateRect);
        GetWindowRect(inner->browser, &browserRect);
        const bool isNarrow = candidateRect.right - candidateRect.left <
                              (browserRect.right - browserRect.left) / 2;
        const bool isBar = candidate != inner->browser &&
                           IsWindowVisible(candidate) &&
                           GetWindowTextLengthW(candidate) == 0 && isNarrow;
        if (isBar) {
          inner->found = candidate;
        }
        return isBar ? FALSE : TRUE;
      },
      reinterpret_cast<LPARAM>(&search));
  return search.found;
}

inline void ApplyBarWidth(Session& session, bool isAsking) {
  HWND bar = BarWindowOf(session.window);
  RECT barRect{};
  RECT browserRect{};
  const bool isMeasured = (bar != nullptr) &&
                          (GetWindowRect(bar, &barRect) != 0) &&
                          (GetWindowRect(session.window, &browserRect) != 0);
  if (!isMeasured) {
    return;
  }
  const int current = barRect.right - barRect.left;
  const bool isHeldWide =
      session.widenedBarWidth != 0 && current == session.widenedBarWidth;
  if (!isHeldWide) {
    session.stockBarWidth = current;
  }
  const int roomForBar =
      browserRect.right - browserRect.left - 2 * kBarMarginPixels;
  const int widest = std::min(kBarMaxWidthPixels, roomForBar);
  const int wanted =
      isAsking
          ? std::max(session.stockBarWidth,
                     std::min(session.stockBarWidth * kAskWidthFactor, widest))
          : session.stockBarWidth;
  session.widenedBarWidth = isAsking ? wanted : 0;
  if (isAsking) {
    // Edge puts its own width back whenever it lays the bar out again; the
    // timer's tick (Tick) holds it.
    StartTimer();
  }
  if (wanted == current) {
    return;
  }
  SetWindowPos(bar, nullptr, barRect.right - wanted, barRect.top, wanted,
               barRect.bottom - barRect.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace edgeplus::find
