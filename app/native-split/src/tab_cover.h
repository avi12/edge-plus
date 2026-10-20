// tab_cover.h — ink on a bright tab. One concern: every vertical tab label is
// one theme text colour, so on a bright cyan fill (the selected tab, and the tab
// under the pointer, which Edge fills the same) the light title, audio icon and
// close icon would vanish. A click-through popup per such tab repaints them in
// ink on the fill's own colour, sampled from the screen so it always matches
// what Edge painted. Edge's views are only read. UI thread only.
#pragma once
#include <windows.h>
#include "edge.h"
#include "geom.h"
#include "vtab_views.h"

namespace fixedge::tab_cover {

using namespace fixedge::edge155;

inline constexpr wchar_t kWindowClass[] = L"FixEdgeTabLabel";
inline constexpr COLORREF kInk = RGB(0x04, 0x12, 0x1A);
inline constexpr int kTitleFontDip = 13;
inline constexpr int kGlyphFontDip = 14;
inline constexpr size_t kTitleCapacity = 256;

// A tab row's children: the title label, the audio indicator and the close button.
inline constexpr int kLabelChild = 0;
inline constexpr int kIndicatorChild = 2;
inline constexpr int kCloseChild = 4;
// Segoe MDL2 Assets: Volume, Cancel.
inline constexpr wchar_t kSpeakerGlyph[] = {0xE767, 0};
inline constexpr wchar_t kCloseGlyph[] = {0xE711, 0};

// A fill this bright (0-255 luma) is one the light label cannot be read on; the
// rail and an idle hover are far darker, a dimmed (unfocused) cyan is just over.
inline constexpr int kBrightLuma = 90;
inline constexpr int kLumaRed = 299;
inline constexpr int kLumaGreen = 587;
inline constexpr int kLumaBlue = 114;
inline constexpr int kLumaScale = 1000;
// Where the fill is sampled: this many DIPs below a tab's top edge, mid-width,
// clear of the covered children.
inline constexpr int kSampleTopDip = 3;
inline constexpr float kHalf = 0.5f;

// Set by the colour hook (hooks.h): the theme's label colour is light, so a
// bright fill needs a cover.
inline bool g_isCoverNeeded = false;

// One cover popup and what it paints; offsets are pixels within the popup.
struct Cover {
  HWND window = nullptr;
  COLORREF fill = 0;
  wchar_t title[kTitleCapacity] = {};
  int indicatorWidth = 0;  // 0 = no audio indicator, drawn at the left
  int titleLeft = 0;
  int titleRight = 0;
  int closeLeft = 0;
  int closeWidth = 0;  // 0 = no close button
};

// Tabs bright at once: the selected one, the one under the pointer, and any left
// mid-way through Edge's hover fade.
inline constexpr size_t kMaxCovers = 8;
inline constexpr size_t kMaxTabs = 128;
inline Cover g_covers[kMaxCovers];
inline HFONT g_titleFont = nullptr;
inline HFONT g_glyphFont = nullptr;
inline int g_fontHeight = 0;

inline void DrawGlyph(HDC context, const wchar_t* glyph, RECT area) {
  DrawTextW(context, glyph, -1, &area,
            DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
}

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  if (message != WM_PAINT) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
  const auto* cover =
      reinterpret_cast<const Cover*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  PAINTSTRUCT paint{};
  HDC context = BeginPaint(hwnd, &paint);
  RECT client{};
  GetClientRect(hwnd, &client);
  const bool isReady = cover && g_titleFont && g_glyphFont;
  if (isReady) {
    HBRUSH fill = CreateSolidBrush(cover->fill);
    FillRect(context, &client, fill);
    DeleteObject(fill);
    SetBkMode(context, TRANSPARENT);
    SetTextColor(context, kInk);
    HGDIOBJ oldFont = SelectObject(context, g_glyphFont);
    if (cover->indicatorWidth > 0) {
      RECT area = client;
      area.right = cover->indicatorWidth;
      DrawGlyph(context, kSpeakerGlyph, area);
    }
    if (cover->closeWidth > 0) {
      RECT area = client;
      area.left = cover->closeLeft;
      area.right = cover->closeLeft + cover->closeWidth;
      DrawGlyph(context, kCloseGlyph, area);
    }
    SelectObject(context, g_titleFont);
    RECT titleArea = client;
    titleArea.left = cover->titleLeft;
    titleArea.right = cover->titleRight;
    DrawTextW(context, cover->title, -1, &titleArea,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(context, oldFont);
  }
  EndPaint(hwnd, &paint);
  return 0;
}

inline HINSTANCE ThisModule() {
  HMODULE thisModule = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&ThisModule), &thisModule);
  return thisModule;
}

// An opaque popup over Edge that the pointer passes straight through. Without
// WS_EX_LAYERED, WS_EX_TRANSPARENT does not pass hits on, so the popup took the
// pointer: Edge saw it leave the tab, ended the hover, the cover went, the
// pointer was back on the tab, and the hover flickered on and off.
inline HWND CreateClickThroughPopup(HWND owner, const wchar_t* className) {
  HWND popup = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
      className, L"", WS_POPUP, 0, 0, 0, 0, owner, nullptr, ThisModule(),
      nullptr);
  if (popup) {
    constexpr BYTE kOpaque = 255;
    SetLayeredWindowAttributes(popup, 0, kOpaque, LWA_ALPHA);
  }
  return popup;
}

inline void EnsureWindow(Cover* cover, HWND owner) {
  if (cover->window) {
    return;
  }
  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = ThisModule();
  windowClass.lpszClassName = kWindowClass;
  RegisterClassExW(&windowClass);  // fails harmlessly if already registered
  cover->window = CreateClickThroughPopup(owner, kWindowClass);
  if (cover->window) {
    SetWindowLongPtrW(cover->window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cover));
  }
}

inline void EnsureFonts(float scale) {
  const int titleHeight = -static_cast<int>(kTitleFontDip * scale);
  if (titleHeight == g_fontHeight && g_titleFont) {
    return;
  }
  DeleteObject(g_titleFont);
  DeleteObject(g_glyphFont);
  g_titleFont =
      CreateFontW(titleHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  g_glyphFont = CreateFontW(
      -static_cast<int>(kGlyphFontDip * scale), 0, 0, 0, FW_NORMAL, FALSE,
      FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
  g_fontHeight = titleHeight;
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

inline bool IsBright(COLORREF color) {
  const int luma =
      (GetRValue(color) * kLumaRed + GetGValue(color) * kLumaGreen +
       GetBValue(color) * kLumaBlue) /
      kLumaScale;
  return luma >= kBrightLuma;
}

// What Edge painted at the top of `tab`, read back from the screen.
inline COLORREF SampleFill(const Rect& tab, float scale) {
  HDC screen = GetDC(nullptr);
  const int sampleX = static_cast<int>((tab.x + tab.width * kHalf) * scale);
  const int sampleY = static_cast<int>((tab.y + kSampleTopDip) * scale);
  const COLORREF color = GetPixel(screen, sampleX, sampleY);
  ReleaseDC(nullptr, screen);
  return color;
}

// A child view's screen bounds when it is shown, else an empty rect.
inline Rect ShownChildBounds(void* tab, int childIndex) {
  const bool isPresent =
      edge::ViewChildCount(tab) > static_cast<size_t>(childIndex);
  void* child = isPresent ? edge::ViewChildAt(tab, childIndex) : nullptr;
  const bool isShown = child && edge::ViewGetVisible(child);
  return isShown ? edge::ViewBoundsInScreen(child) : Rect{};
}

inline void Hide(Cover* cover) {
  if (cover->window) {
    ShowWindow(cover->window, SW_HIDE);
  }
}

// Whether a view has any size on screen (a tab scrolled out of the list has none).
inline bool IsShown(const Rect& bounds) {
  return bounds.width > 0 && bounds.height > 0;
}

// Cover `tab`'s title and icons in ink on `fill`; false when it can't be done
// (the title can't be read), leaving the cover hidden.
inline bool Update(Cover* cover, HWND owner, void* tab, COLORREF fill,
                   float scale) {
  const bool isLabelPresent = edge::ViewChildCount(tab) > kLabelChild;
  void* label = isLabelPresent ? edge::ViewChildAt(tab, kLabelChild) : nullptr;
  const size_t length = label ? ReadTitle(label, cover->title) : 0;
  const Rect labelBounds =
      length > 0 ? edge::ViewBoundsInScreen(label) : Rect{};
  if (!IsShown(labelBounds)) {
    Hide(cover);
    return false;
  }
  EnsureWindow(cover, owner);
  if (!cover->window) {
    return false;
  }
  EnsureFonts(scale);
  // The cover starts at the audio indicator and ends at the close button, when
  // the tab shows them.
  const Rect indicator = ShownChildBounds(tab, kIndicatorChild);
  const Rect close = ShownChildBounds(tab, kCloseChild);
  const bool hasIndicator = indicator.width > 0 && indicator.x < labelBounds.x;
  const bool hasClose =
      close.width > 0 && close.x >= labelBounds.x + labelBounds.width;
  const int left = hasIndicator ? indicator.x : labelBounds.x;
  const int labelRight = labelBounds.x + labelBounds.width;
  const int right = hasClose ? close.x + close.width : labelRight;
  cover->fill = fill;
  cover->indicatorWidth =
      hasIndicator ? static_cast<int>(indicator.width * scale) : 0;
  cover->titleLeft = static_cast<int>((labelBounds.x - left) * scale);
  cover->titleRight = static_cast<int>((labelRight - left) * scale);
  cover->closeWidth = hasClose ? static_cast<int>(close.width * scale) : 0;
  cover->closeLeft = static_cast<int>((close.x - left) * scale);
  SetWindowPos(cover->window, nullptr, static_cast<int>(left * scale),
               static_cast<int>(labelBounds.y * scale),
               static_cast<int>((right - left) * scale),
               static_cast<int>(labelBounds.height * scale),
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
  InvalidateRect(cover->window, nullptr, TRUE);
  return true;
}

// Cover every tab whose fill is bright: Edge paints the selected tab, the hovered
// one and one fading out of hover alike, and all three hide the light title.
inline void UpdateAll(HWND owner, float scale) {
  void* tabs[kMaxTabs];
  const size_t tabCount =
      vtab_views::AllTabs(edge::McvForWindow(owner), tabs, kMaxTabs);
  size_t coverCount = 0;
  for (size_t i = 0; i < tabCount && coverCount < kMaxCovers; i++) {
    const Rect bounds = edge::ViewBoundsInScreen(tabs[i]);
    const bool isCandidate = g_isCoverNeeded && IsShown(bounds);
    const COLORREF fill = isCandidate ? SampleFill(bounds, scale) : 0;
    const bool isCovered =
        isCandidate && IsBright(fill) &&
        Update(&g_covers[coverCount], owner, tabs[i], fill, scale);
    if (isCovered) {
      coverCount++;
    }
  }
  for (size_t i = coverCount; i < kMaxCovers; i++) {
    Hide(&g_covers[i]);
  }
}

inline void HideAll() {
  for (Cover& cover : g_covers) {
    Hide(&cover);
  }
}

// Destroy the popups and fonts (before this DLL unloads).
inline void Remove() {
  for (Cover& cover : g_covers) {
    if (cover.window) {
      DestroyWindow(cover.window);
      cover.window = nullptr;
    }
  }
  DeleteObject(g_titleFont);
  DeleteObject(g_glyphFont);
  g_titleFont = nullptr;
  g_glyphFont = nullptr;
  UnregisterClassW(kWindowClass, ThisModule());
}

}  // namespace fixedge::tab_cover
