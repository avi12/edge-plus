// spine.h — the magenta spine on the selected vertical tab. One concern: a thin
// click-through popup laid over the left edge of the selected tab's row. No ui
// colour id paints a selected-tab outline, and hooking Edge's tab painting
// crashed the browser, so the stripe lives in its own layered window, outside
// Edge's views: a timer reads (never writes) the selected tab view's screen
// bounds and moves the stripe to match. UI thread only.
#pragma once
#include <windows.h>
#include "edge.h"
#include "geom.h"

namespace fixedge::spine {

using namespace fixedge::edge155;

inline constexpr wchar_t kWindowClass[] = L"FixEdgeTabSpine";
inline constexpr UINT_PTR kTimerId = 0xF1F0;
inline constexpr UINT kTickIntervalMs = 100;

// The stripe: 3 DIPs wide, the neon magenta of the Cyberpunk theme.
inline constexpr int kWidthDip = 3;
// Kept clear of the tab pill's rounded corners.
inline constexpr int kCornerInsetDip = 6;
inline constexpr COLORREF kColor = RGB(0xFF, 0x2B, 0xD6);

// A vertical tab sits well below the window's top edge; a horizontal tab strip
// is in the title bar, so tabs above this are not ours to mark.
inline constexpr int kVerticalListTopDip = 48;

inline constexpr int kMaxDepth = 64;

inline HWND g_window = nullptr;
inline bool g_isActive = false;

// Tab rows found so far in the walk, in tree order.
struct TabSearch {
  int wantedIndex = 0;
  int seen = 0;
  void* found = nullptr;
};

inline void FindTab(void* view, int depth, TabSearch* search) {
  const bool isDone = search->found || depth > kMaxDepth;
  if (isDone) {
    return;
  }
  const bool isTab = edge::VtableRva(view) == rva::VerticalTab_vftable;
  if (isTab) {
    if (search->seen == search->wantedIndex) {
      search->found = view;
    }
    search->seen++;
    return;
  }
  const size_t count = edge::ViewChildCount(view);
  for (size_t i = 0; i < count; i++) {
    FindTab(edge::ViewChildAt(view, i), depth + 1, search);
  }
}

// The selected tab's row view, or null when it can't be told.
inline void* SelectedTabView(void* tsm, void* anyView) {
  void* activeTab = edge::ActiveTab(tsm);
  const int index =
      activeTab ? edge::GetIndexOfTabRecursive(edge::RootCollection(tsm), activeTab) : -1;
  if (index < 0) {
    return nullptr;
  }
  TabSearch search{index};
  FindTab(edge::ViewRoot(anyView), 0, &search);
  return search.found;
}

inline HINSTANCE ThisModule() {
  HMODULE thisModule = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&ThisModule), &thisModule);
  return thisModule;
}

inline void EnsureWindow(HWND owner) {
  if (g_window) {
    return;
  }
  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = ThisModule();
  windowClass.hbrBackground = CreateSolidBrush(kColor);
  windowClass.lpszClassName = kWindowClass;
  RegisterClassExW(&windowClass);  // fails harmlessly if already registered
  g_window = CreateWindowExW(
      WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kWindowClass,
      L"", WS_POPUP, 0, 0, 0, 0, owner, nullptr, ThisModule(), nullptr);
}

inline void Hide() {
  if (g_window) {
    ShowWindow(g_window, SW_HIDE);
  }
}

// One tick: put the stripe on the selected vertical tab, or hide it.
inline void Tick(HWND owner) {
  void* tsm = edge::TsmForWindow(owner);
  const bool isWindowShown = IsWindowVisible(owner) && !IsIconic(owner);
  void* anyView = edge::McvForWindow(owner);
  void* tab = tsm && anyView && isWindowShown ? SelectedTabView(tsm, anyView) : nullptr;
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
  EnsureWindow(owner);
  if (!g_window) {
    return;
  }
  const int inset = static_cast<int>(kCornerInsetDip * scale);
  SetWindowPos(g_window, nullptr, static_cast<int>(bounds.x * scale), top + inset,
               static_cast<int>(kWidthDip * scale),
               static_cast<int>(bounds.height * scale) - 2 * inset,
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

inline void Start(HWND owner) {
  g_isActive = true;
  SetTimer(owner, kTimerId, kTickIntervalMs, nullptr);
}

// Stop ticking and drop the stripe (before this DLL unloads).
inline void Remove(HWND owner) {
  g_isActive = false;
  KillTimer(owner, kTimerId);
  if (g_window) {
    DestroyWindow(g_window);
    g_window = nullptr;
  }
  UnregisterClassW(kWindowClass, ThisModule());
}

}  // namespace fixedge::spine
