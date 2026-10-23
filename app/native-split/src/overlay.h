// overlay.h — the drop preview: a translucent rect where the dragged pane will
// land. A click-through layered Win32 popup owned by the browser window, so it
// never enters Edge's view tree or layout. UI thread only.
#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include "geom.h"
#include "handles.h"
#include "motion.h"
#include "theme_tone.h"

namespace fixedge::overlay {

enum class Style : uint8_t {
  kPane,  // beside a pane
  kRoot,  // the whole split's edge
  kSwap
};

inline constexpr wchar_t kWindowClass[] = L"FixEdgeDropPreview";
inline constexpr UINT_PTR kTimerId = 1;
inline constexpr unsigned kMoveMs = 90;
inline constexpr UINT kFrameMs = 15;

// A swap is drawn with a dashed edge.
inline constexpr float kPaneFillAlpha = 0.22f;
inline constexpr float kRootFillAlpha = 0.30f;
inline constexpr float kSwapFillAlpha = 0.12f;
inline constexpr float kEdgeAlpha = 0.9f;
inline constexpr float kBorderDip = 2.f;
inline constexpr float kRootBorderDip = 3.f;
inline constexpr float kCornerRadiusDip = 8.f;
inline constexpr float kDashDip = 8.f;

inline constexpr WORD kBitsPerPixel = 32;  // BGRA

inline constinit HWND g_window = nullptr;
inline constinit bool g_isShown = false;
inline constinit Style g_style = Style::kPane;
inline constinit float g_scale = 1.f;
inline constinit Rect g_from;
inline constinit Rect g_to;
inline constinit Rect g_current;
inline constinit ULONGLONG g_moveStart = 0;

// Premultiplied BGRA.
inline uint32_t Pixel(COLORREF color, float alpha) {
  const auto alphaByte =
      static_cast<uint32_t>(std::lround(alpha * theme_tone::kChannelMax));
  const auto red = GetRValue(color) * alphaByte / theme_tone::kChannelMax;
  const auto green = GetGValue(color) * alphaByte / theme_tone::kChannelMax;
  const auto blue = GetBValue(color) * alphaByte / theme_tone::kChannelMax;
  return (alphaByte << theme_tone::kAlphaShift) |
         (red << theme_tone::kRedShift) | (green << theme_tone::kGreenShift) |
         (blue << theme_tone::kBlueShift);
}

inline constexpr float FillAlpha(Style style) noexcept {
  switch (style) {
    case Style::kRoot:
      return kRootFillAlpha;
    case Style::kSwap:
      return kSwapFillAlpha;
    case Style::kPane:
      break;
  }
  return kPaneFillAlpha;
}

// Negative outside a rounded corner.
inline int DistanceInside(int column, int row, int width, int height,
                          int radius) {
  // Past the nearest corner arc's centre (positive on both axes = within a
  // corner's square).
  const int cornerX = std::max(radius - column, column - (width - 1 - radius));
  const int cornerY = std::max(radius - row, row - (height - 1 - radius));
  const bool isInCorner = cornerX > 0 && cornerY > 0;
  if (isInCorner) {
    const float fromArcCentre =
        std::sqrt(static_cast<float>(cornerX * cornerX + cornerY * cornerY));
    return radius - static_cast<int>(std::lround(fromArcCentre));
  }
  return std::min(std::min(column, width - 1 - column),
                  std::min(row, height - 1 - row));
}

inline void Paint(const Rect& rect) {
  const bool canPaint = g_window && rect.width > 0 && rect.height > 0;
  if (!canPaint) {
    return;
  }
  const float borderDip = g_style == Style::kRoot ? kRootBorderDip : kBorderDip;
  const int border = static_cast<int>(std::lround(borderDip * g_scale));
  const int radius = static_cast<int>(std::lround(kCornerRadiusDip * g_scale));
  const int dash = static_cast<int>(std::lround(kDashDip * g_scale));
  const COLORREF accent = GetSysColor(COLOR_HIGHLIGHT);
  const uint32_t fillPixel = Pixel(accent, FillAlpha(g_style));
  const uint32_t edgePixel = Pixel(accent, kEdgeAlpha);

  BITMAPINFO bitmapInfo{};
  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = rect.width;
  bitmapInfo.bmiHeader.biHeight = -rect.height;  // top-down
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = kBitsPerPixel;
  void* bits = nullptr;
  // Declaration order: given back in reverse, the bitmap deselected before it is
  // deleted.
  const handles::WindowDc screenContext{nullptr};
  const handles::UniqueMemoryDc memoryContext{
      CreateCompatibleDC(screenContext.Get())};
  const handles::UniqueBitmap bitmap{CreateDIBSection(
      screenContext.Get(), &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0)};
  if (!bitmap) {
    return;
  }
  const handles::SelectedObject selection{memoryContext.Get(), bitmap.Get()};
  auto* pixels = static_cast<uint32_t*>(bits);
  for (int row = 0; row < rect.height; row++) {
    for (int column = 0; column < rect.width; column++) {
      const int distanceInside =
          DistanceInside(column, row, rect.width, rect.height, radius);
      uint32_t pixel = 0;
      const bool isInside = distanceInside >= 0;
      if (isInside) {
        const bool isOnEdge = distanceInside < border;
        const bool isDashGap = g_style == Style::kSwap && isOnEdge &&
                               dash > 0 && ((column + row) / dash) % 2;
        const bool isOutline = isOnEdge && !isDashGap;
        pixel = isOutline ? edgePixel : fillPixel;
      }
      pixels[row * rect.width + column] = pixel;
    }
  }
  POINT destination{rect.x, rect.y};
  POINT source{0, 0};
  SIZE size{rect.width, rect.height};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, theme_tone::kChannelMax, AC_SRC_ALPHA};
  UpdateLayeredWindow(g_window, screenContext.Get(), &destination, &size,
                      memoryContext.Get(), &source, 0, &blend, ULW_ALPHA);
}

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  const bool isHitTest = message == WM_NCHITTEST;
  if (isHitTest) {
    return HTTRANSPARENT;
  }
  const bool isMoveFrame = message == WM_TIMER && wparam == kTimerId;
  // KillTimer does not drop a WM_TIMER already queued, and painting a layered
  // window (UpdateLayeredWindow) can put a hidden one back on screen: a frame
  // arriving after Hide left the preview stuck over the panes.
  const bool isStaleFrame = isMoveFrame && !g_isShown;
  if (isStaleFrame) {
    return 0;
  }
  if (isMoveFrame) {
    const double progress =
        static_cast<double>(GetTickCount64() - g_moveStart) / kMoveMs;
    const bool hasArrived = progress >= 1.0;
    g_current =
        hasArrived ? g_to : motion::Lerp(g_from, g_to, motion::Ease(progress));
    Paint(g_current);
    if (hasArrived) {
      KillTimer(hwnd, kTimerId);
    }
    return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

inline HINSTANCE ThisModule() {
  HMODULE thisModule = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&WindowProc), &thisModule);
  return thisModule;
}

// `rect` in screen px.
inline void Show(HWND owner, const Rect& rect, Style style) {
  if (!g_window) {
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = ThisModule();
    windowClass.lpszClassName = kWindowClass;
    RegisterClassExW(&windowClass);  // fails harmlessly if already registered
    g_window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kWindowClass, L"", WS_POPUP, rect.x, rect.y, rect.width, rect.height,
        owner, nullptr, ThisModule(), nullptr);
    if (!g_window) {
      return;
    }
  }
  g_scale =
      static_cast<float>(GetDpiForWindow(owner)) / USER_DEFAULT_SCREEN_DPI;
  const bool isRestyled = style != g_style;
  g_style = style;
  if (!g_isShown) {
    g_to = rect;
    g_current = rect;
    Paint(g_current);
    ShowWindow(g_window, SW_SHOWNOACTIVATE);
    g_isShown = true;
    return;
  }
  const bool isSameTarget = motion::Same(rect, g_to);
  if (isSameTarget) {
    if (isRestyled) {
      Paint(g_current);
    }
    return;
  }
  g_from = g_current;
  g_to = rect;
  g_moveStart = GetTickCount64();
  SetTimer(g_window, kTimerId, kFrameMs, nullptr);
}

inline void Hide() {
  const bool isPreviewShown = g_window && g_isShown;
  if (!isPreviewShown) {
    return;
  }
  KillTimer(g_window, kTimerId);
  ShowWindow(g_window, SW_HIDE);
  g_isShown = false;
}

inline void Remove() {
  if (g_window) {
    DestroyWindow(g_window);
  }
  g_window = nullptr;
  g_isShown = false;
  UnregisterClassW(kWindowClass, ThisModule());
}

}  // namespace fixedge::overlay
