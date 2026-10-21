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
#include <atomic>
#include <vector>
#include "edge.h"
#include "geom.h"
#include "handles.h"
#include "log.h"
#include "tab_edge.h"
#include "theme_tone.h"
#include "vtab_views.h"

namespace fixedge::spine {

// A stripe's class carries its colour as its brush, so there is one class per
// stripe colour (tab_edge::StripeColorFor) and a window whose theme changes tone
// gets a stripe of the other class.
inline constexpr wchar_t kStripeClassOnDark[] = L"FixEdgeTabSpine";
inline constexpr wchar_t kStripeClassOnLight[] = L"FixEdgeTabSpineLight";
inline constexpr wchar_t kCornerClass[] = L"FixEdgeTabCorner";
inline constexpr UINT kTickIntervalMs = 50;

// The stripe: 7 DIPs wide.
inline constexpr int kWidthDip = 7;
// How far across and down the pieces cover each leading corner of the pill:
// Edge rounds a vertical tab by 7 DIPs, and the favicon starts 8 DIPs in, so 8
// covers the curve and its anti-aliased rim without reaching the icon.
inline constexpr int kCornerDip = 8;

// The selected tab's fill as the colour hook publishes it (hooks.h h_getcolor),
// so the corner squares match the pill Edge paints.
inline constinit theme_tone::Argb g_selectedFill = 0;

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
  theme_tone::Argb stripeColor = 0;
};

inline constinit std::vector<WindowSet> g_sets;
inline constinit bool g_isActive = false;
// The thread timer Start set (0 while off).
inline constinit UINT_PTR g_timerId = 0;
// A thread timer outlives every window, so it must be stopped before this DLL
// unloads. The teardown normally does that on the UI thread; when it cannot run
// (the window the DLL attached to has closed), the unload thread asks the next
// tick to stop the timer itself and waits for it (control.h Remove).
inline constinit std::atomic<bool> g_isStopRequested{false};
inline constinit std::atomic<bool> g_isTimerRunning{false};
// The fill the shared corner class brush holds (0 until the hook answers one).
inline constinit theme_tone::Argb g_paintedFill = 0;

inline constexpr COLORREF ColorRefOf(theme_tone::Argb color) noexcept {
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
  handles::UniqueBrush brush{CreateSolidBrush(color)};
  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = ThisModule();
  windowClass.hbrBackground = brush.Get();
  windowClass.lpszClassName = className;
  // A registered class owns its brush (UnregisterClass deletes it). Once the
  // class exists, registering fails harmlessly and the brush stays ours to
  // delete.
  const bool isRegistered = RegisterClassExW(&windowClass) != 0;
  if (isRegistered) {
    brush.Release();
  }
  HWND popup = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
      className, L"", WS_POPUP, 0, 0, 0, 0, owner, nullptr, ThisModule(),
      nullptr);
  if (popup) {
    SetLayeredWindowAttributes(popup, 0, theme_tone::kChannelMax, LWA_ALPHA);
  }
  return popup;
}

// The stripe colour for `browser`'s theme: its tone, read as theme_follow reads
// it (edge::ThemeTone), chooses it.
inline theme_tone::Argb StripeColorOf(HWND browser) {
  void* themeService = edge::ThemeServiceForWindow(browser);
  return tab_edge::StripeColorFor(themeService ? edge::ThemeTone(themeService)
                                               : theme_tone::Tone::kUnknown);
}

inline HWND CreateStripe(HWND browser, theme_tone::Argb color) {
  const bool isLightColor = color == tab_edge::kStripeOnLight;
  return CreatePopup(browser,
                     isLightColor ? kStripeClassOnLight : kStripeClassOnDark,
                     ColorRefOf(color));
}

// The set of `browser` in `stripeColor`, made on first use, its stripe remade
// when the theme's tone has changed the colour. Edge settles its UI direction
// at startup, so each window's side is asked once, when its set is made.
inline WindowSet& SetFor(HWND browser, theme_tone::Argb stripeColor) {
  for (WindowSet& set : g_sets) {
    if (set.browser != browser) {
      continue;
    }
    const bool isRecoloured = set.stripeColor != stripeColor;
    if (isRecoloured) {
      DestroyWindow(set.stripe);
      set.stripe = CreateStripe(browser, stripeColor);
      set.stripeColor = stripeColor;
      Log("[spine] window %p stripe now %08x", browser, stripeColor);
    }
    return set;
  }
  WindowSet set;
  set.browser = browser;
  set.side = tab_edge::LeadingSide(edge::IsUiRightToLeft());
  set.stripeColor = stripeColor;
  set.stripe = CreateStripe(browser, stripeColor);
  set.topCorner = CreatePopup(browser, kCornerClass, ColorRefOf(g_paintedFill));
  set.bottomCorner =
      CreatePopup(browser, kCornerClass, ColorRefOf(g_paintedFill));
  Log("[spine] window %p gets a spine, %s edge, stripe %08x", browser,
      set.side == tab_edge::Side::kRight ? "right" : "left", stripeColor);
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
  const ULONG_PTR oldBrush =
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
// While a tab of this window is dragged, `draggedTab` is its view, and the
// spine rides it instead of the selected tab's row, which stays in the list.
inline void TickWindow(HWND browser, void* draggedTab) {
  void* tsm = edge::TsmForWindow(browser);
  const bool isWindowShown = IsWindowVisible(browser) && !IsIconic(browser);
  void* anyView = edge::McvForWindow(browser);
  void* selectedTab = tsm && anyView && isWindowShown
                          ? vtab_views::SelectedTab(tsm, anyView)
                          : nullptr;
  void* tab = draggedTab && selectedTab ? draggedTab : selectedTab;
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
  const WindowSet& set = SetFor(browser, StripeColorOf(browser));
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

// Start calling `onTick` every tick. A thread timer, not one on a window: the
// browser window the DLL attached to can close while others stay open, and a
// window timer would die with it. Call on the UI thread; `onTick` runs there.
inline void Start(TIMERPROC onTick) {
  g_isActive = true;
  g_timerId = SetTimer(nullptr, 0, kTickIntervalMs, onTick);
  g_isTimerRunning = g_timerId != 0;
  Log("[spine] on (thread timer %llu)",
      static_cast<unsigned long long>(g_timerId));
}

// Stop ticking and drop every window's popups (switched off, or before this DLL
// unloads). On the UI thread that started the timer.
inline void Remove() {
  g_isActive = false;
  if (g_timerId != 0) {
    KillTimer(nullptr, g_timerId);
    g_timerId = 0;
  }
  g_isTimerRunning = false;
  for (const WindowSet& set : g_sets) {
    Destroy(set);
  }
  g_sets.clear();
  UnregisterClassW(kStripeClassOnDark, ThisModule());
  UnregisterClassW(kStripeClassOnLight, ThisModule());
  UnregisterClassW(kCornerClass, ThisModule());
  g_paintedFill = 0;
  Log("[spine] off");
}

}  // namespace fixedge::spine
