// spine.h — the selected vertical tab's magenta spine. One concern: a plain view
// of Edge's own, a child of the tab's view along its leading edge (the left one,
// the right one in a right-to-left UI; tab_edge.h places it): a stripe the full
// height of the row. As a child it is painted and moved by Edge with the tab, in
// the same frame: a stripe in a window of its own trailed a moving tab by a
// frame and flickered. The tab's straight leading edge is tab_shape.h's, so the
// stripe's outer corners are square and the tab's own fill is never painted
// over. No ui colour id paints a selected-tab outline, and hooking Edge's tab
// painting crashed the browser, so nothing here hooks painting: a timer finds
// each window's selected tab view and moves the stripe onto it when it changes.
// The stripe owns nothing of ours (a view, a solid background), so it outlives
// this DLL harmlessly. Which windows are browser windows is control.h's call
// (TickWindow is its per-window step). UI thread only.
#pragma once
#include <windows.h>
#include <atomic>
#include <vector>
#include "edge.h"
#include "geom.h"
#include "log.h"
#include "tab_edge.h"
#include "tab_shape.h"
#include "theme_tone.h"
#include "vtab_views.h"

namespace fixedge::spine {

inline constexpr UINT kTickIntervalMs = 50;

// The stripe: 7 DIPs wide.
inline constexpr int kWidthDip = 7;

// A vertical tab sits well below the window's top edge; a horizontal tab strip
// is in the title bar, so tabs above this are not ours to mark.
inline constexpr int kVerticalListTopDip = 48;

// After a tab drag ends, Edge updates its tab order at once but moves the list's
// rows a few ticks later, so the selected row by index is first a neighbour of
// the dropped tab; the spine marks the slot nearest the drop until the selected
// row is back there, or this long has passed.
inline constexpr ULONGLONG kSettleTimeoutMs = 1500;

// One browser window's spine.
struct WindowSet {
  HWND browser = nullptr;
  tab_edge::Side side = tab_edge::Side::kLeft;
  // Null until first made. While `host` is set it is the host's child (the host
  // owns it and frees it if it dies first); otherwise it is ours to free.
  void* stripe = nullptr;
  void* host = nullptr;  // the tab view it is on; never read, only compared
  // Whether the tabs have been repainted since their outline last changed
  // (tab_shape.h), so they show the straight edge or give it back.
  bool isShapeShown = false;
  theme_tone::Argb stripeColor = 0;  // what the stripe was last painted in
  // Where the spine was, for a drop's settle (screen DIPs, a tab's top).
  bool isDragging = false;
  int lastTabY = 0;
  bool isSettling = false;
  int dropTabY = 0;
  ULONGLONG settleDeadline = 0;
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

// The stripe colour for `browser`'s theme: its tone, read as theme_follow reads
// it (edge::ThemeTone), chooses it.
inline theme_tone::Argb StripeColorOf(HWND browser) {
  void* themeService = edge::ThemeServiceForWindow(browser);
  return tab_edge::StripeColorFor(themeService ? edge::ThemeTone(themeService)
                                               : theme_tone::Tone::kUnknown);
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
  Log("[spine] window %p gets a spine, %s edge", browser,
      set.side == tab_edge::Side::kRight ? "right" : "left");
  g_sets.push_back(set);
  return g_sets.back();
}

// Take the stripe off the tab that holds it; it stays ours. A tab that has
// been destroyed took it along, so it is forgotten. The tree is searched for
// the stripe rather than `host` asked, since `host` may be freed.
inline void Detach(WindowSet& set, void* anyView, void* draggedTab) {
  if (!set.host) {
    return;
  }
  void* holder = vtab_views::ParentHolding(edge::ViewRoot(anyView), set.stripe);
  if (!holder && draggedTab) {
    holder = vtab_views::ParentHolding(edge::ViewRoot(draggedTab), set.stripe);
  }
  set.host = nullptr;
  if (!holder) {
    set.stripe = nullptr;
    return;
  }
  edge::ViewRemoveChild(holder, set.stripe);
}

// Put the stripe on `tab`, unless it is on it already.
inline void Attach(WindowSet& set, void* anyView, void* draggedTab, void* tab) {
  const bool isOnTab =
      set.host == tab && vtab_views::HoldsChild(tab, set.stripe);
  if (isOnTab) {
    return;
  }
  Detach(set, anyView, draggedTab);
  if (!set.stripe) {
    set.stripe = edge::NewPlainView();
    edge::MakeViewDecorative(set.stripe);
    set.stripeColor = 0;
  }
  edge::AddChildViewAt(tab, set.stripe, edge::ViewChildCount(tab));
  set.host = tab;
  const Rect bounds = edge::ViewBounds(tab);
  const Rect onScreen = edge::ViewBoundsInScreen(tab);
  Log("[spine] on a %s tab %p, %d,%d %dx%d, screen %d,%d visible=%d, "
      "children %zu",
      edge::IsHorizontalTabView(tab) ? "horizontal" : "vertical", tab, bounds.x,
      bounds.y, bounds.width, bounds.height, onScreen.x, onScreen.y,
      edge::ViewGetVisible(tab) ? 1 : 0, edge::ViewChildCount(tab));
}

// A horizontal tab's view is wider than the chip Edge draws in it: the flared
// feet and the strip's top padding lie outside the shape, and children are
// clipped to the shape, so the stripe is placed inside it.
inline constexpr int kHorizontalChipSideDip = 10;
inline constexpr int kHorizontalChipTopDip = 4;

// The part of `tab`'s view that holds its fill, in the view's own coordinates.
inline Rect ChipOf(void* tab) {
  const Rect bounds = edge::ViewBounds(tab);
  if (!edge::IsHorizontalTabView(tab)) {
    return Rect{0, 0, bounds.width, bounds.height};
  }
  return Rect{kHorizontalChipSideDip, kHorizontalChipTopDip,
              bounds.width - 2 * kHorizontalChipSideDip,
              bounds.height - kHorizontalChipTopDip};
}

// Size and colour the stripe for `tab`, in its own coordinates.
inline void Lay(WindowSet& set, void* tab, theme_tone::Argb stripeColor) {
  edge::SetViewBounds(set.stripe,
                      tab_edge::StripeFor(ChipOf(tab), set.side, kWidthDip));
  if (set.stripeColor != stripeColor) {
    edge::SetViewSolidBackground(set.stripe, stripeColor);
    set.stripeColor = stripeColor;
  }
  edge::SetViewVisible(set.stripe, true);
}

// Free the stripe of a set that is going away; it must be off any tab.
inline void Free(WindowSet& set) {
  if (set.stripe) {
    edge::DeleteView(set.stripe);
    set.stripe = nullptr;
  }
}

// The start of a tick: drop the sets of browser windows that have closed. A
// window that closed took the stripe on its tab along; a stripe off any tab is
// still ours.
inline void BeginTick() {
  std::erase_if(g_sets, [](WindowSet& set) {
    const bool isClosed = !IsWindow(set.browser);
    if (isClosed && !set.host) {
      Free(set);
    }
    return isClosed;
  });
}

// A window's selected tab view and which tab layout it is in. Edge keeps the
// vertical list's views while the horizontal strip is in use, so a live
// horizontal active tab decides first; the vertical list is the fallback.
struct SelectedTab {
  void* view = nullptr;
  bool isHorizontal = false;
};
inline SelectedTab FindSelectedTab(void* tsm, void* anyView) {
  void* horizontal = vtab_views::ActiveHorizontalTab(edge::ViewRoot(anyView));
  if (horizontal) {
    return {horizontal, true};
  }
  return {vtab_views::SelectedTab(tsm, anyView), false};
}

// One browser window's step of a tick: put the spine on its selected tab (the
// vertical list's or the horizontal strip's), or take it off. While a tab of this window is dragged, `draggedTab` is
// its view, and the spine rides it instead of the selected tab's row, which
// stays in the list.
inline void TickWindow(HWND browser, void* draggedTab) {
  void* tsm = edge::TsmForWindow(browser);
  const bool isWindowShown = IsWindowVisible(browser) && !IsIconic(browser);
  void* anyView = edge::McvForWindow(browser);
  const SelectedTab selected = tsm && anyView && isWindowShown
                                   ? FindSelectedTab(tsm, anyView)
                                   : SelectedTab{};
  void* selectedTab = selected.view;
  const bool isHorizontalStrip = selected.isHorizontal;
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
  const bool isMarkable =
      tab && (isInVerticalList || isHorizontalStrip) && isVisibleRow;
  if (!isMarkable) {
    for (WindowSet& set : g_sets) {
      if (set.browser == browser && anyView) {
        Detach(set, anyView, draggedTab);
      }
    }
    return;
  }
  WindowSet& set = SetFor(browser);
  if (!set.isShapeShown) {
    vtab_views::RepaintTabRows(edge::ViewRoot(anyView));
    edge::SchedulePaint(tab);
    set.isShapeShown = true;
  }
  const bool hasDragEnded = set.isDragging && !draggedTab;
  set.isDragging = draggedTab != nullptr;
  if (hasDragEnded) {
    set.isSettling = true;
    set.dropTabY = set.lastTabY;
    set.settleDeadline = GetTickCount64() + kSettleTimeoutMs;
  }
  // The tab lands in the slot nearest where it was released, and Edge snaps it
  // there at once; until the selected row catches up, the spine marks that slot.
  void* target = tab;
  if (set.isSettling) {
    void* slotTab = vtab_views::TabRowNearest(anyView, set.dropTabY);
    const bool isAtSlot = tab == slotTab;
    const bool isOverdue = GetTickCount64() >= set.settleDeadline;
    const bool isStillSettling = slotTab && !isAtSlot && !isOverdue;
    if (isStillSettling) {
      target = slotTab;
    } else {
      set.isSettling = false;
    }
  }
  Attach(set, anyView, draggedTab, target);
  Lay(set, target, StripeColorOf(browser));
  set.lastTabY = edge::ViewBoundsInScreen(target).y;
}

// Start calling `onTick` every tick. A thread timer, not one on a window: the
// browser window the DLL attached to can close while others stay open, and a
// window timer would die with it. Call on the UI thread; `onTick` runs there.
inline void Start(TIMERPROC onTick) {
  g_isActive = true;
  tab_shape::g_isSquared = true;
  g_timerId = SetTimer(nullptr, 0, kTickIntervalMs, onTick);
  g_isTimerRunning = g_timerId != 0;
  Log("[spine] on (thread timer %llu)",
      static_cast<unsigned long long>(g_timerId));
}

// Stop ticking and take the spine off every window (switched off, or before
// this DLL unloads). On the UI thread that started the timer.
inline void Remove() {
  g_isActive = false;
  tab_shape::g_isSquared = false;
  if (g_timerId != 0) {
    KillTimer(nullptr, g_timerId);
    g_timerId = 0;
  }
  g_isTimerRunning = false;
  for (WindowSet& set : g_sets) {
    void* anyView =
        IsWindow(set.browser) ? edge::McvForWindow(set.browser) : nullptr;
    if (anyView) {
      Detach(set, anyView, nullptr);
      Free(set);
    }
  }
  g_sets.clear();
  Log("[spine] off");
}

}  // namespace fixedge::spine
