// control.h — runs pane actions on Edge's UI thread. One concern: subclass the
// browser window so its hot-keys and the channel's message (channel.h, wParam =
// a Command) run pane actions on the very thread that owns the views (the
// WndProc runs on the UI thread). Hooks queue work there, and scripts post the
// same codes. The original WndProc is restored on unload.
#pragma once
#include <windows.h>
#include <cstring>
#include "channel.h"
#include "edge.h"
#include "split_ops.h"
#include "split_data.h"
#include "state.h"
#include "anim.h"
#include "drag.h"
#include "dividers.h"
#include "grip.h"
#include "links.h"
#include "menu.h"
#include "offer.h"
#include "overlay.h"
#include "tabdrag.h"

namespace fixedge::control {

using channel::CodeOf;
using channel::Command;
using channel::IsInRange;
using channel::OperandOf;

inline WNDPROC g_originalProc = nullptr;
inline bool g_hasHotKeys = false;

inline constexpr wchar_t kBrowserWindowClass[] = L"Chrome_WidgetWin_1";
inline constexpr int kClassNameLength = 64;
inline constexpr UINT kTeardownTimeoutMs = 5000;

// Hot-key ids, delivered as WM_HOTKEY's wParam to this window's (UI-thread)
// WndProc.
enum class HotKey : WPARAM {
  kAddPane = 1,
  kCollapse = 2,
  kLeft = 3,
  kUp = 4,
  kRight = 5,
  kDown = 6,
  kSwap = 7,
};

struct HotKeyBinding {
  HotKey hotKey;
  UINT key;
};

// Every hot-key is Ctrl+Shift+Alt + its key; registered (and unregistered) in
// this order.
inline constexpr UINT kHotKeyModifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
inline constexpr HotKeyBinding kHotKeyBindings[] = {
    {HotKey::kAddPane, 'A'},     // add a pane to the right
    {HotKey::kCollapse, 'X'},    // collapse the split to the active pane
    {HotKey::kLeft, VK_LEFT},    // add a pane to the left
    {HotKey::kUp, VK_UP},        // add a pane above
    {HotKey::kRight, VK_RIGHT},  // add a pane to the right
    {HotKey::kDown, VK_DOWN},    // add a pane below
    {HotKey::kSwap, 'S'},        // swap the active pane with its counterpart
};

inline bool HasBinding(WPARAM wparam) {
  for (const HotKeyBinding& binding : kHotKeyBindings) {
    const bool isBoundHotKey = static_cast<WPARAM>(binding.hotKey) == wparam;
    if (isBoundHotKey) {
      return true;
    }
  }
  return false;
}

inline void RegisterHotKeys(HWND hwnd) {
  g_hasHotKeys = true;
  for (const HotKeyBinding& binding : kHotKeyBindings) {
    RegisterHotKey(hwnd, static_cast<int>(binding.hotKey), kHotKeyModifiers,
                   binding.key);
  }
  Log("[control] hotkeys: Ctrl+Shift+Alt + arrows=add, A=add right, S=swap, "
      "X=collapse");
}

inline void UnregisterHotKeys(HWND hwnd) {
  for (const HotKeyBinding& binding : kHotKeyBindings) {
    UnregisterHotKey(hwnd, static_cast<int>(binding.hotKey));
  }
}

// Everything that must be undone on the UI thread before unload: the drag
// session, the preview window, the grips, the animation timer and the hot-keys.
inline void Teardown(HWND hwnd) {
  drag::Cancel();
  tabdrag::Cancel();
  overlay::Remove();
  grip::RemoveAll();
  dividers::RemoveAll();
  anim::Remove();
  if (g_hasHotKeys) {
    UnregisterHotKeys(hwnd);
  }
  g_hasHotKeys = false;
}

// --- the hot-keys ----------------------------------------------------------

inline void CollapseByHotKey(HWND hwnd) {
  void* tsm = edge::TsmForWindow(hwnd);
  if (tsm) {
    split_data::CollapseSplit(tsm);
  }
  // A collapsed split starts a fresh tree.
  ForgetTree(edge::McvForWindow(hwnd));
}

inline void AddPaneByHotKey(HWND hwnd, Direction direction) {
  void* mcv = edge::McvForWindow(hwnd);
  Log("[control] directional add dir=%d mcv=%p", static_cast<int>(direction),
      mcv);
  if (mcv) {
    split_ops::AddPane(mcv, direction);
  }
}

inline void RunHotKey(HWND hwnd, HotKey hotKey) {
  switch (hotKey) {
    case HotKey::kCollapse:
      CollapseByHotKey(hwnd);
      break;
    case HotKey::kSwap:
      split_ops::SwapActive(edge::McvForWindow(hwnd));
      break;
    case HotKey::kAddPane:
    case HotKey::kRight:
      AddPaneByHotKey(hwnd, Direction::kRight);
      break;
    case HotKey::kLeft:
      AddPaneByHotKey(hwnd, Direction::kLeft);
      break;
    case HotKey::kUp:
      AddPaneByHotKey(hwnd, Direction::kUp);
      break;
    case HotKey::kDown:
      AddPaneByHotKey(hwnd, Direction::kDown);
      break;
  }
}

// --- the channel's commands ------------------------------------------------

inline void AddPaneByCommand(HWND hwnd, WPARAM wparam) {
  void* mcv = edge::McvForWindow(hwnd);
  Log("[control] msg directional dir=%llu mcv=%p",
      static_cast<unsigned long long>(wparam) - CodeOf(Command::kAddPaneBase),
      mcv);
  if (mcv) {
    split_ops::AddPane(
        mcv, static_cast<Direction>(wparam - CodeOf(Command::kAddPaneBase)));
  }
}

inline void EnsureGripsByCommand(HWND hwnd) {
  if (void* mcv = edge::McvForWindow(hwnd)) {
    grip::EnsureGrips(mcv);
    dividers::Ensure(mcv);
  }
}

inline void ShowBarByCommand(HWND hwnd, int pane) {
  void* mcv = edge::McvForWindow(hwnd);
  const bool hasPane = mcv && pane < static_cast<int>(edge::McvCount(mcv));
  if (hasPane) {
    void* webContents =
        edge::ContainerWebContents(edge::McvContainerAt(mcv, pane));
    const bool isShown = edge::ShowControlButtonFor(
        edge::BrowserOf(edge::McvBrowserView(mcv)), webContents);
    Log("[control] show bar pane=%d wc=%p -> %d", pane, webContents, isShown);
  }
}

// A code no command claims: create a split of N panes or of two, or grow one.
inline void CreateOrGrowByCommand(HWND hwnd, WPARAM wparam) {
  void* tsm = edge::TsmForWindow(hwnd);
  Log("[control] message wparam=%llu tsm=%p",
      static_cast<unsigned long long>(wparam), tsm);
  if (!tsm) {
    return;
  }
  const bool isCreatePanes = wparam >= CodeOf(Command::kCreatePanesBase);
  if (isCreatePanes) {
    split_data::CreateSplitN(tsm, OperandOf(wparam, Command::kCreatePanesBase));
    return;
  }
  const bool isCreateSplit = wparam >= CodeOf(Command::kCreateSplit);
  if (isCreateSplit) {
    split_data::CreateSplit(tsm);
    return;
  }
  split_data::GrowSplit(tsm);
}

inline void RunCommand(HWND hwnd, WPARAM wparam) {
  const bool isMenuArrow =
      IsInRange(wparam, Command::kMenuArrowBase, channel::kDirectionCount);
  if (isMenuArrow) {
    menu::RunArrow(
        static_cast<Direction>(OperandOf(wparam, Command::kMenuArrowBase)));
    return;
  }
  const bool isDirectionalAdd =
      IsInRange(wparam, Command::kAddPaneBase, channel::kDirectionCount);
  if (isDirectionalAdd) {
    AddPaneByCommand(hwnd, wparam);
    return;
  }
  switch (static_cast<Command>(wparam)) {
    case Command::kSwap:
      split_ops::SwapActive(edge::McvForWindow(hwnd));
      return;
    case Command::kEnsureGrips:
      EnsureGripsByCommand(hwnd);
      return;
    case Command::kTeardown:
      Teardown(hwnd);
      return;
    case Command::kTabDrop:
      tabdrag::ApplyDrop();
      return;
    case Command::kLinkPane:
      links::OpenLinkPane();
      return;
    case Command::kCloseOfferPane:
      offer::CloseOfferPane();
      return;
    default:
      break;
  }
  const bool isShowBar =
      IsInRange(wparam, Command::kShowBarBase, channel::kShowBarPaneLimit);
  if (isShowBar) {
    ShowBarByCommand(hwnd, OperandOf(wparam, Command::kShowBarBase));
    return;
  }
  CreateOrGrowByCommand(hwnd, wparam);
}

// --- the window procedure --------------------------------------------------

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  // RegisterHotKey must run on the thread that owns the window; this WndProc is
  // exactly that thread, so register lazily on the first message we see.
  if (!g_hasHotKeys) {
    RegisterHotKeys(hwnd);
  }
  // Escape cancels a pane drag (the drag itself arrives through views; hooks.h).
  const bool isEscapeDuringDrag =
      message == WM_KEYDOWN && wparam == VK_ESCAPE && drag::IsActive();
  if (isEscapeDuringDrag) {
    drag::Cancel();
    return 0;
  }
  const bool isAnimationFrame = message == WM_TIMER && wparam == anim::kTimerId;
  if (isAnimationFrame) {
    anim::Tick();
    return 0;
  }
  const bool isOurHotKey = message == WM_HOTKEY && HasBinding(wparam);
  if (isOurHotKey) {
    RunHotKey(hwnd, static_cast<HotKey>(wparam));
    return 0;
  }
  const bool isCommand =
      message == channel::g_message && channel::g_message != 0;
  if (isCommand) {
    RunCommand(hwnd, wparam);
    return 0;
  }
  return CallWindowProcW(g_originalProc, hwnd, message, wparam, lparam);
}

// EnumWindows callback: claim the first visible, titled browser window of this
// process.
inline BOOL CALLBACK FindBrowserWindow(HWND hwnd, LPARAM /*unused*/) {
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  const bool isOurProcess = processId == GetCurrentProcessId();
  if (!isOurProcess) {
    return TRUE;
  }
  const bool isVisibleWithTitle =
      IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) != 0;
  if (!isVisibleWithTitle) {
    return TRUE;
  }
  wchar_t className[kClassNameLength] = {};
  GetClassNameW(hwnd, className, kClassNameLength);
  const bool isBrowserWindow = wcscmp(className, kBrowserWindowClass) == 0;
  if (!isBrowserWindow) {
    return TRUE;
  }
  channel::g_window = hwnd;
  return FALSE;
}

inline void Install() {
  channel::g_message = RegisterWindowMessageW(channel::kMessageName);
  EnumWindows(FindBrowserWindow, 0);
  HWND hwnd = channel::g_window;
  if (!hwnd) {
    Log("[control] no browser window to subclass");
    return;
  }
  g_originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
      hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
  Log("[control] subclassed hwnd=%p msg=%u", hwnd, channel::g_message);
  anim::Install(hwnd);
  // A (re)load into a split that is already grown: add the grips on the UI thread.
  channel::Post(CodeOf(Command::kEnsureGrips));
}

// Runs on the unload thread: hand the UI-side teardown to the UI thread and wait
// for it, then restore the original window procedure.
inline void Remove() {
  HWND hwnd = channel::g_window;
  const bool isSubclassed = hwnd && g_originalProc;
  if (isSubclassed) {
    SendMessageTimeoutW(hwnd, channel::g_message, CodeOf(Command::kTeardown), 0,
                        SMTO_BLOCK, kTeardownTimeoutMs, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(g_originalProc));
  }
  g_hasHotKeys = false;
  channel::g_window = nullptr;
  g_originalProc = nullptr;
}

}  // namespace fixedge::control
