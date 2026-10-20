// spine.h — the selected vertical tab's square leading edge and its magenta
// spine. One concern: thin click-through popups laid over the leading edge of
// the selected tab's row (the left one, the right one in a right-to-left UI;
// tab_edge.h places them): the stripe the full height of the row, and a square
// of the selected fill over each of the pill's rounded corners on that side, so
// the edge reads straight with the spine flush on it. Every browser window gets
// its own set, owned by it, so it rides that window's z-order and dies with it.
// No ui colour id paints a selected-tab outline, and hooking Edge's tab painting
// crashed the browser, so these live in their own layered windows, outside
// Edge's views: a timer reads (never writes) each window's selected tab view's
// screen bounds and moves that window's popups to match. Which windows are
// browser windows is control.h's call (TickWindow is its per-window step). UI
// thread only.
#pragma once
#include <windows.h>
#include <vector>
#include "edge.h"
#include "geom.h"
#include "log.h"
#include "tab_edge.h"
#include "theme_tone.h"
#include "vtab_views.h"

namespace fixedge::spine {

inline constexpr wchar_t kWindowClass[] = L"FixEdgeTabSpine";
inline constexpr wchar_t kCornerClass[] = L"FixEdgeTabCorner";
inline constexpr UINT_PTR kTimerId = 0xF1F0;
inline constexpr UINT kTickIntervalMs = 50;

// The stripe: 7 DIPs wide, the neon magenta of the Cyberpunk theme.
inline constexpr int kWidthDip = 7;
inline constexpr COLORREF kColor = RGB(0xFF, 0x2B, 0xD6);
// How far across and down the pieces cover each leading corner of the pill:
// Edge rounds a vertical tab by 7 DIPs, and the favicon starts 8 DIPs in, so 8
// covers the curve and its anti-aliased rim without reaching the icon.
inline constexpr int kCornerDip = 8;

// The selected tab's fill as the colour hook publishes it (hooks.h h_getcolor),
// so the corner squares match the pill Edge paints.
inline theme_tone::Argb g_selectedFill = 0;

// A vertical tab sits well below the window's top edge; a horizontal tab strip
// is in the title bar, so tabs above this are not ours to mark.
inline constexpr int kVerticalListTopDip = 48;

// One browser window's stripe and two corner squares: plain solid popups, each
// a class brush and nothing else, the one kind of popup proven to show over
// Edge. Owned by `browser`, so Windows destroys them when it closes.
struct WindowSet {
  HWND browser = nullptr;
  HWND stripe = nullptr;
  HWND topCorner = nullptr;
  HWND bottomCorner = nullptr;
  tab_edge::Side side = tab_edge::Side::kLeft;
};

inline std::vector<WindowSet> g_sets;
inline bool g_isActive = false;
// The fill the shared corner class brush holds (0 until the hook answers one).
inline theme_tone::Argb g_paintedFill = 0;

inline COLORREF ColorRefOf(theme_tone::Argb color) {
  return RGB((color >> theme_tone::kRedShift) & theme_tone::kChannelMask,
             (color >> theme_tone::kGreenShift) & theme_tone::kChannelMask,
             (color >> theme_tone::kBlueShift) & theme_tone::kChannelMask);
}

inline HINSTANCE ThisModule() {
  HMODULE thisModule = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&ThisModule), &thisModule);
  return thisModule;
}

// An opaque popup over Edge, painted in its class brush, that the pointer
// passes straight through. Without WS_EX_LAYERED, WS_EX_TRANSPARENT does not
// pass hits on, so the popup would take the pointer: Edge would see it leave
// the tab and end the hover.
inline HWND CreatePopup(HWND owner, const wchar_t* className, COLORREF color) {
  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = ThisModule();
  windowClass.hbrBackground = CreateSolidBrush(color);
  windowClass.lpszClassName = className;
  RegisterClassExW(&windowClass);  // fails harmlessly if already registered
  HWND popup = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
      className, L"", WS_POPUP, 0, 0, 0, 0, owner, nullptr, ThisModule(),
      nullptr);
  if (popup) {
    SetLayeredWindowAttributes(popup, 0, theme_tone::kChannelMax, LWA_ALPHA);
  }
  return popup;
}

// The set of `browser`, made on first use. Edge settles its UI direction at
// startup, so each window's side is asked once, when its set is made.
inline WindowSet& SetFor(HWND browser) {
  for (WindowSet& set : g_sets) {
    if (set.browser == browser) {
      return set;
    }
  }
  WindowSet set;
  set.browser = browser;
  set.side = tab_edge::LeadingSide(edge::IsUiRightToLeft());
  set.stripe = CreatePopup(browser, kWindowClass, kColor);
  set.topCorner = CreatePopup(browser, kCornerClass, ColorRefOf(g_paintedFill));
  set.bottomCorner =
      CreatePopup(browser, kCornerClass, ColorRefOf(g_paintedFill));
  Log("[spine] window %p gets a spine, %s edge", browser,
      set.side == tab_edge::Side::kRight ? "right" : "left");
  g_sets.push_back(set);
  return g_sets.back();
}

// Repaint every corner in the selected fill once it has changed. The corners
// share one class, so one brush swap recolours them all.
inline void FollowFill() {
  const bool hasFillChanged = g_paintedFill != g_selectedFill;
  if (!hasFillChanged || g_sets.empty()) {
    return;
  }
  g_paintedFill = g_selectedFill;
  HBRUSH brush = CreateSolidBrush(ColorRefOf(g_paintedFill));
  const LONG_PTR oldBrush =
      SetClassLongPtrW(g_sets.front().topCorner, GCLP_HBRBACKGROUND,
                       reinterpret_cast<LONG_PTR>(brush));
  DeleteObject(reinterpret_cast<HBRUSH>(oldBrush));
  for (const WindowSet& set : g_sets) {
    InvalidateRect(set.topCorner, nullptr, TRUE);
    InvalidateRect(set.bottomCorner, nullptr, TRUE);
  }
  Log("[spine] corners filled %08x", g_paintedFill);
}

inline void Place(HWND popup, const Rect& rect) {
  if (popup) {
    SetWindowPos(popup, nullptr, rect.x, rect.y, rect.width, rect.height,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
  }
}

inline void Hide(const WindowSet& set) {
  for (HWND popup : {set.stripe, set.topCorner, set.bottomCorner}) {
    if (popup) {
      ShowWindow(popup, SW_HIDE);
    }
  }
}

inline void Destroy(const WindowSet& set) {
  for (HWND popup : {set.stripe, set.topCorner, set.bottomCorner}) {
    if (popup) {
      DestroyWindow(popup);
    }
  }
}

// The start of a tick: drop the sets of browser windows that have closed
// (Windows already destroyed their owned popups).
inline void BeginTick() {
  std::erase_if(g_sets, [](const WindowSet& set) {
    return !IsWindow(set.browser);
  });
}

// One browser window's step of a tick: square its selected vertical tab's
// leading edge and put the stripe on it, or hide them.
inline void TickWindow(HWND browser) {
  void* tsm = edge::TsmForWindow(browser);
  const bool isWindowShown = IsWindowVisible(browser) && !IsIconic(browser);
  void* anyView = edge::McvForWindow(browser);
  void* tab = tsm && anyView && isWindowShown
                  ? vtab_views::SelectedTab(tsm, anyView)
                  : nullptr;
  const float scale =
      static_cast<float>(GetDpiForWindow(browser)) / USER_DEFAULT_SCREEN_DPI;
  RECT windowRect{};
  GetWindowRect(browser, &windowRect);
  const Rect bounds = tab ? edge::ViewBoundsInScreen(tab) : Rect{};
  const int top = static_cast<int>(static_cast<float>(bounds.y) * scale);
  const bool isInVerticalList =
      top - windowRect.top > static_cast<int>(kVerticalListTopDip * scale);
  const bool isVisibleRow = bounds.width > 0 && bounds.height > 0;
  const bool isMarkable = tab && isInVerticalList && isVisibleRow;
  if (!isMarkable) {
    for (const WindowSet& set : g_sets) {
      if (set.browser == browser) {
        Hide(set);
      }
    }
    return;
  }
  const WindowSet& set = SetFor(browser);
  FollowFill();
  const Rect row{static_cast<int>(static_cast<float>(bounds.x) * scale), top,
                 static_cast<int>(static_cast<float>(bounds.width) * scale),
                 static_cast<int>(static_cast<float>(bounds.height) * scale)};
  const tab_edge::Pieces pieces =
      tab_edge::PiecesFor(row, set.side, static_cast<int>(kWidthDip * scale),
                          static_cast<int>(kCornerDip * scale));
  Place(set.stripe, pieces.stripe);
  // Until the colour hook has answered a fill, the corners stay hidden rather
  // than show a square of the wrong colour.
  const bool isFillKnown = g_paintedFill != 0;
  if (!isFillKnown) {
    return;
  }
  Place(set.topCorner, pieces.topCorner);
  Place(set.bottomCorner, pieces.bottomCorner);
}

// Start ticking on `timerWindow` (the DLL's own browser window, control.h).
inline void Start(HWND timerWindow) {
  g_isActive = true;
  Log("[spine] on");
  SetTimer(timerWindow, kTimerId, kTickIntervalMs, nullptr);
}

// Stop ticking and drop every window's popups (switched off, or before this DLL
// unloads).
inline void Remove(HWND timerWindow) {
  g_isActive = false;
  KillTimer(timerWindow, kTimerId);
  for (const WindowSet& set : g_sets) {
    Destroy(set);
  }
  g_sets.clear();
  UnregisterClassW(kWindowClass, ThisModule());
  UnregisterClassW(kCornerClass, ThisModule());
  g_paintedFill = 0;
  Log("[spine] off");
}

}  // namespace fixedge::spine
