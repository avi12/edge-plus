// control.h — a control channel into the DLL that runs pane actions on Edge's UI
// thread. One concern: subclass the browser window so a registered window message
// ("fixedge.addpane", wParam = Direction) triggers split_ops::AddPane on the very
// thread that owns the views (the WndProc runs on the UI thread). This drives
// directional-add independent of Edge's own menu, and lets a script (or a future
// hotkey) post the message. The original WndProc is restored on unload.
#pragma once
#include <windows.h>
#include <cstring>
#include "edge.h"
#include "split_ops.h"
#include "split_data.h"
#include "state.h"
#include "anim.h"
#include "drag.h"
#include "grip.h"
#include "overlay.h"

namespace fixedge::control {

inline UINT g_msg = 0;
inline HWND g_hwnd = nullptr;
inline WNDPROC g_orig_proc = nullptr;
inline bool g_hotkeys_done = false;

// Hot-key ids delivered as WM_HOTKEY to this window's (UI-thread) WndProc.
enum { HK_ADD_PANE = 1, HK_COLLAPSE = 2, HK_LEFT = 3, HK_UP = 4, HK_RIGHT = 5, HK_DOWN = 6, HK_SWAP = 7 };
inline constexpr UINT kHkMods = MOD_CONTROL | MOD_SHIFT | MOD_ALT;

// Channel commands (wParam of "fixedge.addpane") beyond the add-pane test codes.
enum : WPARAM {
  kCmdEnsureGrips = 700,  // give every pane of a grown split its drag grip
  kCmdTeardown = 701,     // undo everything UI-side before this DLL unloads
};

// Everything that must be undone on the UI thread before unload: the drag
// session, the preview window, the grips, the animation timer and the hot-keys.
inline void Teardown(HWND hwnd) {
  drag::Cancel();
  overlay::Remove();
  grip::RemoveAll();
  anim::Remove();
  if (g_hotkeys_done) {
    for (int id = HK_ADD_PANE; id <= HK_SWAP; ++id) UnregisterHotKey(hwnd, id);
  }
  g_hotkeys_done = false;
}

inline LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  // RegisterHotKey must run on the thread that owns the window; this WndProc is
  // exactly that thread, so register lazily on the first message we see.
  if (!g_hotkeys_done) {
    g_hotkeys_done = true;
    // Ctrl+Shift+Alt+A = add a pane to the current split (create one if none).
    RegisterHotKey(hwnd, HK_ADD_PANE, kHkMods, 'A');
    RegisterHotKey(hwnd, HK_COLLAPSE, kHkMods, 'X');
    RegisterHotKey(hwnd, HK_LEFT, kHkMods, VK_LEFT);
    RegisterHotKey(hwnd, HK_UP, kHkMods, VK_UP);
    RegisterHotKey(hwnd, HK_RIGHT, kHkMods, VK_RIGHT);
    RegisterHotKey(hwnd, HK_DOWN, kHkMods, VK_DOWN);
    RegisterHotKey(hwnd, HK_SWAP, kHkMods, 'S');
    Log("[control] hotkeys: Ctrl+Shift+Alt + arrows=add, A=add right, S=swap, X=collapse");
  }
  // A pane drag owns the mouse from a grip press to its release (drag.h).
  switch (msg) {
    case WM_LBUTTONDOWN:
      if (drag::OnButtonDown(hwnd, lparam)) return 0;
      break;
    case WM_MOUSEMOVE:
      if (drag::OnMouseMove(hwnd, lparam)) return 0;
      break;
    case WM_LBUTTONUP:
      if (drag::OnButtonUp(hwnd, lparam)) return 0;
      break;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE && drag::Active()) {
        drag::Cancel();
        return 0;
      }
      break;
    case WM_CAPTURECHANGED:
      if (drag::Active() && reinterpret_cast<HWND>(lparam) != hwnd) drag::Cancel();
      break;
    case WM_SETCURSOR:
      if (drag::Dragging()) {
        SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZEALL)));
        return TRUE;
      }
      break;
    default:
      break;
  }
  if (msg == WM_TIMER && wparam == anim::kTimerId) {
    anim::Tick();
    return 0;
  }
  if (msg == WM_HOTKEY) {
    if (wparam == HK_COLLAPSE) {
      void* tsm = edge::TsmForWindow(hwnd);
      if (tsm) split_data::CollapseSplit(tsm);
      ForgetTree(edge::McvForWindow(hwnd));  // a collapsed split starts a fresh tree
      return 0;
    }
    if (wparam == HK_SWAP) {
      split_ops::SwapActive(edge::McvForWindow(hwnd));
      return 0;
    }
    bool is_add = true;
    fixedge::Direction dir = fixedge::Direction::kRight;
    switch (wparam) {
      case HK_ADD_PANE:
      case HK_RIGHT: dir = fixedge::Direction::kRight; break;
      case HK_LEFT: dir = fixedge::Direction::kLeft; break;
      case HK_UP: dir = fixedge::Direction::kUp; break;
      case HK_DOWN: dir = fixedge::Direction::kDown; break;
      default: is_add = false; break;
    }
    if (is_add) {
      void* mcv = edge::McvForWindow(hwnd);
      Log("[control] directional add dir=%d mcv=%p", static_cast<int>(dir), mcv);
      if (mcv) split_ops::AddPane(mcv, dir);
      return 0;
    }
  }
  if (msg == g_msg && g_msg != 0) {
    // wparam >= 300 = directional add (300+Direction); >= 200 create N panes;
    // >= 100 create a 2-pane split; otherwise grow. (A test channel; the real
    // gestures are the hot-keys.)
    if (wparam >= 300 && wparam <= 303) {
      void* mcv = edge::McvForWindow(hwnd);
      Log("[control] msg directional dir=%llu mcv=%p",
          static_cast<unsigned long long>(wparam) - 300, mcv);
      if (mcv) split_ops::AddPane(mcv, static_cast<fixedge::Direction>(wparam - 300));
      return 0;
    }
    if (wparam == 400) {  // swap active pane with its counterpart
      split_ops::SwapActive(edge::McvForWindow(hwnd));
      return 0;
    }
    if (wparam == kCmdEnsureGrips) {
      if (void* mcv = edge::McvForWindow(hwnd)) grip::EnsureGrips(mcv);
      return 0;
    }
    if (wparam == kCmdTeardown) {
      Teardown(hwnd);
      return 0;
    }
    if (wparam >= 500 && wparam < 516) {  // show pane (wparam-500)'s hover bar
      void* mcv = edge::McvForWindow(hwnd);
      const int pane = static_cast<int>(wparam) - 500;
      if (mcv && pane < static_cast<int>(edge::McvCount(mcv))) {
        void* wc = edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
        const bool shown = edge::ShowControlButtonFor(edge::BrowserOf(edge::McvBrowserView(mcv)), wc);
        Log("[control] show bar pane=%d wc=%p -> %d", pane, wc, shown);
      }
      return 0;
    }
    void* tsm = edge::TsmForWindow(hwnd);
    Log("[control] message wparam=%llu tsm=%p", static_cast<unsigned long long>(wparam), tsm);
    if (!tsm) return 0;
    if (wparam >= 200) {
      split_data::CreateSplitN(tsm, static_cast<int>(wparam) - 200);  // direct AddToSplitImpl, N panes
    } else if (wparam >= 100) {
      split_data::CreateSplit(tsm);  // proven 2-pane AddToNewSplit
    } else {
      split_data::GrowSplit(tsm);
    }
    return 0;
  }
  return CallWindowProcW(g_orig_proc, hwnd, msg, wparam, lparam);
}

inline BOOL CALLBACK EnumCb(HWND hwnd, LPARAM) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != GetCurrentProcessId()) return TRUE;
  if (!IsWindowVisible(hwnd) || GetWindowTextLengthW(hwnd) == 0) return TRUE;
  wchar_t cls[64] = {};
  GetClassNameW(hwnd, cls, 64);
  if (wcscmp(cls, L"Chrome_WidgetWin_1") != 0) return TRUE;
  g_hwnd = hwnd;  // first visible, titled browser window
  return FALSE;
}

inline void Install() {
  g_msg = RegisterWindowMessageW(L"fixedge.addpane");
  EnumWindows(EnumCb, 0);
  if (!g_hwnd) {
    Log("[control] no browser window to subclass");
    return;
  }
  g_orig_proc = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Proc)));
  Log("[control] subclassed hwnd=%p msg=%u", g_hwnd, g_msg);
  anim::Install(g_hwnd);
  // A (re)load into a split that is already grown: add the grips on the UI thread.
  PostMessageW(g_hwnd, g_msg, kCmdEnsureGrips, 0);
}

// Runs on the unload thread: hand the UI-side teardown to the UI thread and wait
// for it, then restore the original window procedure.
inline void Remove() {
  if (g_hwnd && g_orig_proc) {
    SendMessageTimeoutW(g_hwnd, g_msg, kCmdTeardown, 0, SMTO_BLOCK, 5000, nullptr);
  }
  if (g_hwnd && g_orig_proc) {
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig_proc));
  }
  g_hotkeys_done = false;
  g_hwnd = nullptr;
  g_orig_proc = nullptr;
}

}  // namespace fixedge::control
