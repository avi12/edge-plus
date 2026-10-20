// tab_cover.h — ink on the selected tab. One concern: every vertical tab label
// is one theme text colour, so on the selected tab's bright cyan fill the light
// title, audio icon and close icon would vanish. A click-through popup repaints
// them in ink on the fill the colour hook (hooks.h) answered for the current
// focus, so it switches with Edge's own fill when the window gains or loses
// focus. Edge's views are only read. UI thread only.
#pragma once
#include <windows.h>
#include "edge.h"
#include "focus.h"
#include "geom.h"
#include "theme_tone.h"
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

// Set by the colour hook (hooks.h): the theme's label colour is light, so the
// selected fill needs a cover; and the fill it answers for a focused and an
// unfocused window (ARGB, 0 until Edge has first painted the selected tab).
inline bool g_isCoverNeeded = false;
inline uint32_t g_focusedFill = 0;
inline uint32_t g_unfocusedFill = 0;

inline void PublishSelectedFill(uint32_t focusedFill, uint32_t unfocusedFill) {
  g_focusedFill = focusedFill;
  g_unfocusedFill = unfocusedFill;
}

inline COLORREF ToColorRef(theme_tone::Argb color) {
  using theme_tone::kBlueShift;
  using theme_tone::kChannelMask;
  using theme_tone::kGreenShift;
  using theme_tone::kRedShift;
  return RGB((color >> kRedShift) & kChannelMask,
             (color >> kGreenShift) & kChannelMask,
             (color >> kBlueShift) & kChannelMask);
}

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

inline Cover g_cover;
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

// Cover the selected tab on the fill Edge paints it with right now, or hide.
inline void UpdateSelected(HWND owner, void* selectedTab, float scale) {
  const uint32_t fill =
      focus::IsEdgeForeground() ? g_focusedFill : g_unfocusedFill;
  const bool isCoverable = g_isCoverNeeded && fill != 0;
  if (!isCoverable) {
    Hide(&g_cover);
    return;
  }
  Update(&g_cover, owner, selectedTab, ToColorRef(fill), scale);
}

inline void HideAll() {
  Hide(&g_cover);
}

// Destroy the popup and fonts (before this DLL unloads).
inline void Remove() {
  if (g_cover.window) {
    DestroyWindow(g_cover.window);
    g_cover.window = nullptr;
  }
  DeleteObject(g_titleFont);
  DeleteObject(g_glyphFont);
  g_titleFont = nullptr;
  g_glyphFont = nullptr;
  UnregisterClassW(kWindowClass, ThisModule());
}

}  // namespace fixedge::tab_cover
