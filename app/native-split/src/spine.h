// spine.h — the selected vertical tab's square leading edge and its magenta
// spine. One concern: three plain views of Edge's own, children of the tab's
// view along its leading edge (the left one, the right one in a right-to-left
// UI; tab_edge.h places them): the stripe the full height of the row, and a
// square of the selected fill over each of the pill's rounded corners on that
// side, so the edge reads straight with the spine flush on it. As children they
// are painted and moved by Edge with the tab, in the same frame: pieces in
// windows of their own trailed a moving tab by a frame and flickered. No ui
// colour id paints a selected-tab outline, and hooking Edge's tab painting
// crashed the browser, so nothing here hooks painting: a timer finds each
// window's selected tab view and moves the pieces onto it when it changes. The
// pieces own nothing of ours (a view, a solid background), so they outlive this
// DLL harmlessly. Which windows are browser windows is control.h's call
// (TickWindow is its per-window step). UI thread only.
#pragma once
#include <windows.h>
#include <array>
#include <atomic>
#include <vector>
#include "edge.h"
#include "geom.h"
#include "log.h"
#include "tab_edge.h"
#include "theme_tone.h"
#include "vtab_views.h"

namespace fixedge::spine {

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

// After a tab drag ends, Edge updates its tab order at once but moves the list's
// rows a few ticks later, so the selected row by index is first a neighbour of
// the dropped tab; the spine marks the slot nearest the drop until the selected
// row is back there, or this long has passed.
inline constexpr ULONGLONG kSettleTimeoutMs = 1500;

// The pieces of one set, in the order they are made and added.
enum class Piece {
  kStripe,
  kTopCorner,
  kBottomCorner,
  kCount,
};
inline constexpr size_t kPieceCount = static_cast<size_t>(Piece::kCount);

// One browser window's spine.
struct WindowSet {
  HWND browser = nullptr;
  tab_edge::Side side = tab_edge::Side::kLeft;
  // Null until first made. While `host` is set they are its children (it owns
  // them and frees them if it dies first); otherwise they are ours to free.
  std::array<void*, kPieceCount> pieces{};
  void* host = nullptr;  // the tab view they are on; never read, only compared
  theme_tone::Argb stripeColor = 0;  // what the pieces were last painted in
  theme_tone::Argb cornerColor = 0;
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

inline void MakePieces(WindowSet& set) {
  for (void*& piece : set.pieces) {
    piece = edge::NewPlainView();
    edge::MakeViewDecorative(piece);
  }
  set.stripeColor = 0;
  set.cornerColor = 0;
}

// Take the pieces off the tab that holds them; they stay ours. A holder that
// has been destroyed took them along, so they are forgotten. The tree is
// searched for them rather than `host` asked, since `host` may be freed.
inline void Detach(WindowSet& set, void* anyView, void* draggedTab) {
  if (!set.host) {
    return;
  }
  void* stripe = set.pieces[static_cast<size_t>(Piece::kStripe)];
  void* holder = vtab_views::ParentHolding(edge::ViewRoot(anyView), stripe);
  if (!holder && draggedTab) {
    holder = vtab_views::ParentHolding(edge::ViewRoot(draggedTab), stripe);
  }
  set.host = nullptr;
  if (!holder) {
    set.pieces = {};
    return;
  }
  for (void* piece : set.pieces) {
    edge::ViewRemoveChild(holder, piece);
  }
}

// Put the pieces on `target`, unless they are on it already.
inline void Attach(WindowSet& set, void* anyView, void* draggedTab,
                   void* target) {
  const bool isOnTarget =
      set.host == target &&
      vtab_views::HoldsChild(target,
                             set.pieces[static_cast<size_t>(Piece::kStripe)]);
  if (isOnTarget) {
    return;
  }
  Detach(set, anyView, draggedTab);
  if (!set.pieces[static_cast<size_t>(Piece::kStripe)]) {
    MakePieces(set);
  }
  for (void* piece : set.pieces) {
    edge::AddChildViewAt(target, piece, edge::ViewChildCount(target));
  }
  set.host = target;
}

// Size and colour the pieces for `target`, in its own coordinates. Until the
// colour hook has answered a fill, the corners stay hidden rather than show a
// square of the wrong colour.
inline void Lay(WindowSet& set, void* target, theme_tone::Argb stripeColor) {
  const Rect bounds = edge::ViewBounds(target);
  const tab_edge::Pieces layout = tab_edge::PiecesFor(
      Rect{0, 0, bounds.width, bounds.height}, set.side, kWidthDip, kCornerDip);
  void* stripe = set.pieces[static_cast<size_t>(Piece::kStripe)];
  void* topCorner = set.pieces[static_cast<size_t>(Piece::kTopCorner)];
  void* bottomCorner = set.pieces[static_cast<size_t>(Piece::kBottomCorner)];
  edge::SetViewBounds(stripe, layout.stripe);
  edge::SetViewBounds(topCorner, layout.topCorner);
  edge::SetViewBounds(bottomCorner, layout.bottomCorner);
  if (set.stripeColor != stripeColor) {
    edge::SetViewSolidBackground(stripe, stripeColor);
    set.stripeColor = stripeColor;
  }
  const bool isFillKnown = g_selectedFill != 0;
  const bool isFillChanged = isFillKnown && set.cornerColor != g_selectedFill;
  if (isFillChanged) {
    edge::SetViewSolidBackground(topCorner, g_selectedFill);
    edge::SetViewSolidBackground(bottomCorner, g_selectedFill);
    set.cornerColor = g_selectedFill;
  }
  edge::SetViewVisible(stripe, true);
  edge::SetViewVisible(topCorner, isFillKnown);
  edge::SetViewVisible(bottomCorner, isFillKnown);
}

// Free the pieces of a set that is going away; they must be off any tab.
inline void Free(WindowSet& set) {
  for (void*& piece : set.pieces) {
    if (piece) {
      edge::DeleteView(piece);
      piece = nullptr;
    }
  }
}

// The start of a tick: drop the sets of browser windows that have closed. A
// window that closed took the pieces on its tabs along; pieces off any tab
// are still ours.
inline void BeginTick() {
  std::erase_if(g_sets, [](WindowSet& set) {
    const bool isClosed = !IsWindow(set.browser);
    if (isClosed && !set.host) {
      Free(set);
    }
    return isClosed;
  });
}

// One browser window's step of a tick: put the spine on its selected vertical
// tab, or take it off. While a tab of this window is dragged, `draggedTab` is
// its view, and the spine rides it instead of the selected tab's row, which
// stays in the list.
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
    for (WindowSet& set : g_sets) {
      if (set.browser == browser && anyView) {
        Detach(set, anyView, draggedTab);
      }
    }
    return;
  }
  WindowSet& set = SetFor(browser);
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
  g_timerId = SetTimer(nullptr, 0, kTickIntervalMs, onTick);
  g_isTimerRunning = g_timerId != 0;
  Log("[spine] on (thread timer %llu)",
      static_cast<unsigned long long>(g_timerId));
}

// Stop ticking and take the spine off every window (switched off, or before
// this DLL unloads). On the UI thread that started the timer.
inline void Remove() {
  g_isActive = false;
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
