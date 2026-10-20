// spine.h — the selected vertical tab's square leading edge and its magenta
// spine. One concern: thin click-through popups laid over the leading edge of
// the selected tab's row (the left one, the right one in a right-to-left UI;
// tab_edge.h places them): the stripe the full height of the row, and a square
// of the selected fill over each of the pill's rounded corners on that side, so
// the edge reads straight with the spine flush on it. No ui colour id paints a
// selected-tab outline, and hooking Edge's tab painting crashed the browser, so
// these live in their own layered windows, outside Edge's views: a timer reads
// (never writes) the selected tab view's screen bounds and moves the popups to
// match. UI thread only.
#pragma once
#include <windows.h>
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

// The stripe and the two corner squares: plain solid popups, each a class
// brush and nothing else, the one kind of popup proven to show over Edge.
inline HWND g_window = nullptr;
inline HWND g_topCorner = nullptr;
inline HWND g_bottomCorner = nullptr;
inline bool g_isActive = false;
inline theme_tone::Argb g_paintedFill = 0;
// The tab edge the pieces go on, from Edge's UI direction (Start).
inline tab_edge::Side g_side = tab_edge::Side::kLeft;

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

inline void EnsureWindows(HWND owner) {
  if (!g_window) {
    g_window = CreatePopup(owner, kWindowClass, kColor);
  }
  if (!g_topCorner) {
    g_topCorner = CreatePopup(owner, kCornerClass, ColorRefOf(g_paintedFill));
  }
  if (!g_bottomCorner) {
    g_bottomCorner =
        CreatePopup(owner, kCornerClass, ColorRefOf(g_paintedFill));
  }
}

// Repaint the corners in the selected fill once it has changed.
inline void FollowFill() {
  const bool hasFillChanged = g_paintedFill != g_selectedFill;
  if (!hasFillChanged || !g_topCorner) {
    return;
  }
  g_paintedFill = g_selectedFill;
  HBRUSH brush = CreateSolidBrush(ColorRefOf(g_paintedFill));
  const LONG_PTR oldBrush = SetClassLongPtrW(g_topCorner, GCLP_HBRBACKGROUND,
                                             reinterpret_cast<LONG_PTR>(brush));
  DeleteObject(reinterpret_cast<HBRUSH>(oldBrush));
  InvalidateRect(g_topCorner, nullptr, TRUE);
  InvalidateRect(g_bottomCorner, nullptr, TRUE);
  Log("[spine] corners filled %08x", g_paintedFill);
}

inline void Place(HWND popup, const Rect& rect) {
  if (popup) {
    SetWindowPos(popup, nullptr, rect.x, rect.y, rect.width, rect.height,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
  }
}

inline void Hide() {
  for (HWND popup : {g_window, g_topCorner, g_bottomCorner}) {
    if (popup) {
      ShowWindow(popup, SW_HIDE);
    }
  }
}

// One tick: square the selected vertical tab's left edge and put the stripe on
// it, or hide them.
inline void Tick(HWND owner) {
  void* tsm = edge::TsmForWindow(owner);
  const bool isWindowShown = IsWindowVisible(owner) && !IsIconic(owner);
  void* anyView = edge::McvForWindow(owner);
  void* tab = tsm && anyView && isWindowShown
                  ? vtab_views::SelectedTab(tsm, anyView)
                  : nullptr;
  if (!tab) {
    Hide();
    return;
  }
  const float scale =
      static_cast<float>(GetDpiForWindow(owner)) / USER_DEFAULT_SCREEN_DPI;
  RECT windowRect{};
  GetWindowRect(owner, &windowRect);
  const Rect bounds = edge::ViewBoundsInScreen(tab);
  const int top = static_cast<int>(static_cast<float>(bounds.y) * scale);
  const bool isInVerticalList =
      top - windowRect.top > static_cast<int>(kVerticalListTopDip * scale);
  const bool isVisibleRow = bounds.width > 0 && bounds.height > 0;
  if (!isInVerticalList || !isVisibleRow) {
    Hide();
    return;
  }
  EnsureWindows(owner);
  FollowFill();
  const Rect row{static_cast<int>(static_cast<float>(bounds.x) * scale), top,
                 static_cast<int>(static_cast<float>(bounds.width) * scale),
                 static_cast<int>(static_cast<float>(bounds.height) * scale)};
  const tab_edge::Pieces pieces =
      tab_edge::PiecesFor(row, g_side, static_cast<int>(kWidthDip * scale),
                          static_cast<int>(kCornerDip * scale));
  Place(g_window, pieces.stripe);
  // Until the colour hook has answered a fill, the corners stay hidden rather
  // than show a square of the wrong colour.
  const bool isFillKnown = g_paintedFill != 0;
  if (!isFillKnown) {
    return;
  }
  Place(g_topCorner, pieces.topCorner);
  Place(g_bottomCorner, pieces.bottomCorner);
}

inline void Start(HWND owner) {
  g_isActive = true;
  // Edge settles its UI direction at startup, so it is asked once.
  g_side = tab_edge::LeadingSide(edge::IsUiRightToLeft());
  Log("[spine] on, %s edge",
      g_side == tab_edge::Side::kRight ? "right" : "left");
  SetTimer(owner, kTimerId, kTickIntervalMs, nullptr);
}

// Stop ticking and drop the stripe (before this DLL unloads).
inline void Remove(HWND owner) {
  g_isActive = false;
  KillTimer(owner, kTimerId);
  for (HWND* popup : {&g_window, &g_topCorner, &g_bottomCorner}) {
    if (*popup) {
      DestroyWindow(*popup);
      *popup = nullptr;
    }
  }
  UnregisterClassW(kWindowClass, ThisModule());
  UnregisterClassW(kCornerClass, ThisModule());
  g_paintedFill = 0;
}

}  // namespace fixedge::spine
