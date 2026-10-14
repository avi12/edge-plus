// overlay.h — the drop preview. One concern: show one translucent rect over the
// split (screen pixels) where the dragged pane will land, easing between targets.
// A click-through layered Win32 popup owned by the browser window, so it never
// enters Edge's view tree or layout. UI thread only.
#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include "geom.h"
#include "motion.h"

namespace fixedge::overlay {

// What the rect means: beside a pane, the whole split's edge, or a swap.
enum class Style : uint8_t {
  kPane,
  kRoot,
  kSwap
};

inline constexpr wchar_t kClass[] = L"FixEdgeDropPreview";
inline constexpr UINT_PTR kTimerId = 1;
inline constexpr unsigned kMoveMs =
    90;  // ease between targets (not on first show)

inline HWND g_wnd = nullptr;
inline bool g_shown = false;
inline Style g_style = Style::kPane;
inline float g_scale = 1.f;
inline Rect g_from, g_to, g_now;
inline ULONGLONG g_start = 0;

// Premultiplied BGRA pixel of the system accent at `alpha` (0..1).
inline uint32_t Pixel(COLORREF c, float alpha) {
  const auto a = static_cast<uint32_t>(alpha * 255.f + 0.5f);
  const auto r = GetRValue(c) * a / 255, g = GetGValue(c) * a / 255,
             b = GetBValue(c) * a / 255;
  return (a << 24) | (r << 16) | (g << 8) | b;
}

// Render `r` (screen px) into the layered window: a rounded, tinted fill with an
// outline; the whole-split edge is bolder, a swap is fainter with a dashed edge.
inline void Paint(const Rect& r) {
  if (!g_wnd || r.width <= 0 || r.height <= 0) {
    return;
  }
  const float fill_a = g_style == Style::kRoot   ? 0.30f
                       : g_style == Style::kSwap ? 0.12f
                                                 : 0.22f;
  const int border = static_cast<int>(
      std::lround((g_style == Style::kRoot ? 3.f : 2.f) * g_scale));
  const int radius = static_cast<int>(std::lround(8.f * g_scale));
  const int dash = static_cast<int>(std::lround(8.f * g_scale));
  const COLORREF accent = GetSysColor(COLOR_HIGHLIGHT);
  const uint32_t fill = Pixel(accent, fill_a), edge = Pixel(accent, 0.9f);

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = r.width;
  bi.bmiHeader.biHeight = -r.height;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP dib =
      CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib) {
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  auto* px = static_cast<uint32_t*>(bits);
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      // Distance inside the rounded rect's edge (negative = outside the corner).
      const int dx = std::max(radius - x, x - (r.width - 1 - radius));
      const int dy = std::max(radius - y, y - (r.height - 1 - radius));
      int inside;
      if (dx > 0 && dy > 0) {
        inside = radius - static_cast<int>(
                              std::lround(std::sqrt(float(dx * dx + dy * dy))));
      } else {
        inside = std::min(std::min(x, r.width - 1 - x),
                          std::min(y, r.height - 1 - y));
      }
      uint32_t v = 0;
      if (inside >= 0) {
        const bool on_edge = inside < border;
        const bool gap = g_style == Style::kSwap && on_edge && dash > 0 &&
                         ((x + y) / dash) % 2;
        v = on_edge && !gap ? edge : fill;
      }
      px[y * r.width + x] = v;
    }
  }
  POINT dst{r.x, r.y}, src{0, 0};
  SIZE size{r.width, r.height};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(g_wnd, screen, &dst, &size, mem, &src, 0, &blend,
                      ULW_ALPHA);
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}

inline LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wparam,
                             LPARAM lparam) {
  if (msg == WM_NCHITTEST) {
    return HTTRANSPARENT;
  }
  if (msg == WM_TIMER && wparam == kTimerId) {
    const double p = static_cast<double>(GetTickCount64() - g_start) / kMoveMs;
    g_now = p >= 1.0 ? g_to : motion::Lerp(g_from, g_to, motion::Ease(p));
    Paint(g_now);
    if (p >= 1.0) {
      KillTimer(hwnd, kTimerId);
    }
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

inline HINSTANCE Module() {
  HMODULE m = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&Proc), &m);
  return m;
}

// Show the preview at `r` (screen px) over `owner`; moves ease from the current rect.
inline void Show(HWND owner, const Rect& r, Style style) {
  if (!g_wnd) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = Proc;
    wc.hInstance = Module();
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);  // fails harmlessly if already registered
    g_wnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClass, L"", WS_POPUP, r.x, r.y, r.width, r.height, owner, nullptr,
        Module(), nullptr);
    if (!g_wnd) {
      return;
    }
  }
  g_scale = static_cast<float>(GetDpiForWindow(owner)) / 96.f;
  const bool restyle = style != g_style;
  g_style = style;
  if (!g_shown) {  // first appearance: no slide-in
    g_now = g_to = r;
    Paint(g_now);
    ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
    g_shown = true;
    return;
  }
  if (motion::Same(r, g_to)) {
    if (restyle) {
      Paint(g_now);
    }
    return;
  }
  g_from = g_now;
  g_to = r;
  g_start = GetTickCount64();
  SetTimer(g_wnd, kTimerId, 15, nullptr);
}

inline void Hide() {
  if (!g_wnd || !g_shown) {
    return;
  }
  KillTimer(g_wnd, kTimerId);
  ShowWindow(g_wnd, SW_HIDE);
  g_shown = false;
}

// Destroy the window and its class (before this DLL unloads).
inline void Remove() {
  if (g_wnd) {
    DestroyWindow(g_wnd);
  }
  g_wnd = nullptr;
  g_shown = false;
  UnregisterClassW(kClass, Module());
}

}  // namespace fixedge::overlay
