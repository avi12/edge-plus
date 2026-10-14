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

namespace fixedge::control {

inline UINT g_msg = 0;
inline HWND g_hwnd = nullptr;
inline WNDPROC g_orig_proc = nullptr;
inline bool g_hotkeys_done = false;

// Hot-key ids delivered as WM_HOTKEY to this window's (UI-thread) WndProc.
enum { HK_ADD_PANE = 1 };

inline LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  // RegisterHotKey must run on the thread that owns the window; this WndProc is
  // exactly that thread, so register lazily on the first message we see.
  if (!g_hotkeys_done) {
    g_hotkeys_done = true;
    // Ctrl+Shift+Alt+A = add a pane to the current split (create one if none).
    if (RegisterHotKey(hwnd, HK_ADD_PANE, MOD_CONTROL | MOD_SHIFT | MOD_ALT, 'A'))
      Log("[control] hotkey Ctrl+Shift+Alt+A -> add pane registered");
    else
      Log("[control] RegisterHotKey failed (%lu)", GetLastError());
  }
  if (msg == WM_HOTKEY && wparam == HK_ADD_PANE) {
    void* tsm = edge::TsmForWindow(hwnd);
    Log("[control] hotkey add-pane tsm=%p", tsm);
    if (tsm) split_data::GrowSplit(tsm);
    return 0;
  }
  if (msg == g_msg && g_msg != 0) {
    // tsm via HWND -> aura -> BrowserView, so no split need pre-exist.
    // wparam >= 100 = create a fresh split; otherwise grow the active split.
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
}

inline void Remove() {
  if (g_hwnd && g_hotkeys_done) UnregisterHotKey(g_hwnd, HK_ADD_PANE);
  if (g_hwnd && g_orig_proc) {
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig_proc));
  }
  g_hotkeys_done = false;
  g_hwnd = nullptr;
  g_orig_proc = nullptr;
}

}  // namespace fixedge::control
