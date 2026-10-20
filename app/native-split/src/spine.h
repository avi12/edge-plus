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
inline constexpr UINT kTickIntervalMs = 50;

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

// The selected tab's title in black. Every vertical tab label is one theme text
// colour, so on a bright cyan fill the light label would vanish; a second popup
// over the label repaints the title in ink on the fill's own colour. The colour
// hook (hooks.h) publishes the fill it answered and whether the label is light
// (needs the cover).
inline constexpr wchar_t kLabelClass[] = L"FixEdgeTabLabel";
inline constexpr COLORREF kLabelInk = RGB(0x04, 0x12, 0x1A);
inline constexpr int kLabelFontDip = 13;
inline constexpr size_t kTitleCapacity = 256;
inline constexpr int kArgbRedShift = 16;
inline constexpr int kArgbGreenShift = 8;
inline constexpr uint32_t kByteMask = 0xff;
inline uint32_t g_selectedFill = 0;  // ARGB, as the hook last answered
inline bool g_isLabelCoverNeeded = false;
inline HWND g_labelWindow = nullptr;
inline HFONT g_labelFont = nullptr;
inline int g_labelFontHeight = 0;
inline wchar_t g_labelText[kTitleCapacity] = {};
// The tab's audio indicator (a speaker glyph Edge paints in the light label
// colour) is covered and redrawn too: its width in pixels at the cover's left
// (0 = none), and where the title starts after it.
inline constexpr int kIndicatorChild = 2;
inline constexpr wchar_t kSpeakerGlyph[] = {0xE767,
                                            0};  // Segoe MDL2 Assets: Volume
inline constexpr int kIndicatorGlyphDip = 14;
inline constexpr int kCloseChild = 4;
inline constexpr wchar_t kCloseGlyph[] = {0xE711,
                                          0};  // Segoe MDL2 Assets: Cancel
inline int g_indicatorWidth = 0;
inline int g_closeOffset = 0;  // the close button's left in the cover, px
inline int g_closeWidth = 0;   // 0 = no close button
inline int g_titleRight = 0;   // where the title stops, px
inline int g_titleOffset = 0;
inline HFONT g_iconFont = nullptr;

inline COLORREF FillColorRef() {
  return RGB((g_selectedFill >> kArgbRedShift) & kByteMask,
             (g_selectedFill >> kArgbGreenShift) & kByteMask,
             g_selectedFill & kByteMask);
}

inline LRESULT CALLBACK LabelProc(HWND hwnd, UINT message, WPARAM wparam,
                                  LPARAM lparam) {
  if (message != WM_PAINT) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
  PAINTSTRUCT paint{};
  HDC context = BeginPaint(hwnd, &paint);
  RECT client{};
  GetClientRect(hwnd, &client);
  HBRUSH fill = CreateSolidBrush(FillColorRef());
  FillRect(context, &client, fill);
  DeleteObject(fill);
  HGDIOBJ oldFont = SelectObject(context, g_labelFont);
  SetBkMode(context, TRANSPARENT);
  SetTextColor(context, kLabelInk);
  const bool hasIndicator = g_indicatorWidth > 0;
  const bool hasClose = g_closeWidth > 0;
  SelectObject(context, g_iconFont);
  if (hasIndicator) {
    RECT glyphRect = client;
    glyphRect.right = g_indicatorWidth;
    DrawTextW(context, kSpeakerGlyph, -1, &glyphRect,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
  }
  if (hasClose) {
    RECT glyphRect = client;
    glyphRect.left = g_closeOffset;
    glyphRect.right = g_closeOffset + g_closeWidth;
    DrawTextW(context, kCloseGlyph, -1, &glyphRect,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
  }
  SelectObject(context, g_labelFont);
  RECT titleRect = client;
  titleRect.left = g_titleOffset;
  titleRect.right = g_titleRight;
  DrawTextW(context, g_labelText, -1, &titleRect,
            DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
  SelectObject(context, oldFont);
  EndPaint(hwnd, &paint);
  return 0;
}

// The label's title, copied out of its RenderText; 0 when it can't be read.
inline size_t ReadTitle(void* label, wchar_t* out) {
  __try {
    const auto* renderText = *reinterpret_cast<const uint8_t* const*>(
        static_cast<const uint8_t*>(label) + off::Label_render_text);
    if (!renderText) {
      return 0;
    }
    const auto text = edge::U16StringView(renderText + off::RenderText_text);
    const size_t length =
        text.length < kTitleCapacity - 1 ? text.length : kTitleCapacity - 1;
    for (size_t i = 0; i < length; i++) {
      out[i] = static_cast<wchar_t>(text.data[i]);
    }
    out[length] = 0;
    return length;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

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
  const int index = activeTab ? edge::GetIndexOfTabRecursive(
                                    edge::RootCollection(tsm), activeTab)
                              : -1;
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

inline void HideLabelCover() {
  if (g_labelWindow) {
    ShowWindow(g_labelWindow, SW_HIDE);
  }
}

inline void Hide() {
  if (g_window) {
    ShowWindow(g_window, SW_HIDE);
  }
  HideLabelCover();
}

inline void EnsureLabelWindow(HWND owner) {
  if (g_labelWindow) {
    return;
  }
  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.lpfnWndProc = LabelProc;
  windowClass.hInstance = ThisModule();
  windowClass.lpszClassName = kLabelClass;
  RegisterClassExW(&windowClass);  // fails harmlessly if already registered
  g_labelWindow = CreateWindowExW(
      WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kLabelClass, L"",
      WS_POPUP, 0, 0, 0, 0, owner, nullptr, ThisModule(), nullptr);
}

// Repaint the selected tab's title in ink over its (light) label, when the fill
// behind it is bright enough that the light label would not read.
inline void CoverLabel(HWND owner, void* tab, float scale) {
  const bool isLabelPresent = edge::ViewChildCount(tab) > 0;
  void* label = isLabelPresent ? edge::ViewChildAt(tab, 0) : nullptr;
  const size_t length = label ? ReadTitle(label, g_labelText) : 0;
  const Rect bounds = label ? edge::ViewBoundsInScreen(label) : Rect{};
  const bool isCoverable = g_isLabelCoverNeeded && length > 0 &&
                           bounds.width > 0 && bounds.height > 0;
  if (!isCoverable) {
    HideLabelCover();
    return;
  }
  EnsureLabelWindow(owner);
  if (!g_labelWindow) {
    return;
  }
  const int fontHeight = -static_cast<int>(kLabelFontDip * scale);
  if (fontHeight != g_labelFontHeight || !g_labelFont) {
    DeleteObject(g_labelFont);
    DeleteObject(g_iconFont);
    g_labelFont =
        CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_iconFont =
        CreateFontW(-static_cast<int>(kIndicatorGlyphDip * scale), 0, 0, 0,
                    FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH, L"Segoe MDL2 Assets");
    g_labelFontHeight = fontHeight;
  }
  // The cover starts at the audio indicator when the tab shows one.
  const bool isIndicatorChildPresent =
      edge::ViewChildCount(tab) > kIndicatorChild;
  void* indicator = isIndicatorChildPresent
                        ? edge::ViewChildAt(tab, kIndicatorChild)
                        : nullptr;
  const bool isIndicatorShown = indicator && edge::ViewGetVisible(indicator);
  const Rect indicatorBounds =
      isIndicatorShown ? edge::ViewBoundsInScreen(indicator) : Rect{};
  const bool hasIndicator = isIndicatorShown && indicatorBounds.width > 0 &&
                            indicatorBounds.x < bounds.x;
  // ...and ends at the close button when it shows.
  const bool isCloseChildPresent = edge::ViewChildCount(tab) > kCloseChild;
  void* close =
      isCloseChildPresent ? edge::ViewChildAt(tab, kCloseChild) : nullptr;
  const bool isCloseShown = close && edge::ViewGetVisible(close);
  const Rect closeBounds =
      isCloseShown ? edge::ViewBoundsInScreen(close) : Rect{};
  const bool hasClose = isCloseShown && closeBounds.width > 0 &&
                        closeBounds.x >= bounds.x + bounds.width;
  const int left = hasIndicator ? indicatorBounds.x : bounds.x;
  const int right =
      hasClose ? closeBounds.x + closeBounds.width : bounds.x + bounds.width;
  g_closeWidth = hasClose ? static_cast<int>(closeBounds.width * scale) : 0;
  g_closeOffset = static_cast<int>((closeBounds.x - left) * scale);
  g_titleRight = static_cast<int>((bounds.x + bounds.width - left) * scale);
  g_indicatorWidth =
      hasIndicator ? static_cast<int>(indicatorBounds.width * scale) : 0;
  g_titleOffset = static_cast<int>((bounds.x - left) * scale);
  SetWindowPos(g_labelWindow, nullptr, static_cast<int>(left * scale),
               static_cast<int>(bounds.y * scale),
               static_cast<int>((right - left) * scale),
               static_cast<int>(bounds.height * scale),
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
  InvalidateRect(g_labelWindow, nullptr, TRUE);
}

// One tick: put the stripe on the selected vertical tab, or hide it.
inline void Tick(HWND owner) {
  void* tsm = edge::TsmForWindow(owner);
  const bool isWindowShown = IsWindowVisible(owner) && !IsIconic(owner);
  void* anyView = edge::McvForWindow(owner);
  void* tab =
      tsm && anyView && isWindowShown ? SelectedTabView(tsm, anyView) : nullptr;
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
  SetWindowPos(g_window, nullptr, static_cast<int>(bounds.x * scale),
               top + inset, static_cast<int>(kWidthDip * scale),
               static_cast<int>(bounds.height * scale) - 2 * inset,
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
  CoverLabel(owner, tab, scale);
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
  if (g_labelWindow) {
    DestroyWindow(g_labelWindow);
    g_labelWindow = nullptr;
  }
  DeleteObject(g_labelFont);
  g_labelFont = nullptr;
  DeleteObject(g_iconFont);
  g_iconFont = nullptr;
  UnregisterClassW(kWindowClass, ThisModule());
  UnregisterClassW(kLabelClass, ThisModule());
}

}  // namespace fixedge::spine
