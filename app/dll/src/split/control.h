// control.h — runs pane actions on Edge's UI thread: subclasses the browser
// window so its hot-keys and the channel's message (channel.h) run on the thread
// that owns the views, and sends each to its handler in split/control/.
#pragma once
#include <windows.h>
#include <cstring>
#include "chrome/colours/colour_dump.h"
#include "chrome/pane/drag.h"
#include "chrome/pane/menu.h"
#include "chrome/tabs/tabdrag.h"
#include "chrome/tabs/tabrows.h"
#include "core/channel.h"
#include "core/log.h"
#include "edge/api/browser.h"
#include "edge/offsets.h"
#include "find/find.h"
#include "layout/anim.h"
#include "layout/dock.h"
#include "layout/pane_tree.h"
#include "layout/repaint.h"
#include "layout/state.h"
#include "split/control/appearance.h"
#include "split/control/browser_windows.h"
#include "split/control/config_apply.h"
#include "split/control/hotkeys.h"
#include "split/control/split_commands.h"
#include "split/control/teardown.h"
#include "split/links.h"
#include "split/offer.h"
#include "split/split_ops.h"
#include "theme/reading/reading_mode.h"
#include "theme/remix/pages/remix_skin_mode.h"
#include "theme/theme.h"

namespace edgeplus::control {

using channel::CodeOf;
using channel::Command;
using channel::IsInRange;
using channel::OperandOf;

inline constinit WNDPROC g_originalProc = nullptr;

inline constexpr wchar_t kColorSchemeSetting[] = L"ImmersiveColorSet";
inline constexpr UINT kTeardownTimeoutMs = 5000;

// Runs on dllmain's waiter thread. Posting to our own subclassed window means the
// app never has to guess which window we hooked.
inline bool AdoptBrowserWindow(HWND excluded);

inline void OnConfigChanged() {
  // Every browser window closed with Edge still running (in the background):
  // the next one opened takes the channel, so this apply reaches it.
  const bool isWithoutWindow = channel::g_window == nullptr;
  if (isWithoutWindow) {
    AdoptBrowserWindow(nullptr);
  }
  g_hasPendingConfig = LoadPendingConfig();
  if (g_hasPendingConfig) {
    channel::Post(CodeOf(Command::kApplyConfig));
  }
}

inline void RunCommand(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (wparam == CodeOf(Command::kRedrawFollowedPages)) {
    RefreshThemeColors();
    return;
  }
  if (wparam == CodeOf(Command::kReloadConfig)) {
    OnConfigChanged();
    return;
  }
  if (wparam == CodeOf(Command::kApplyConfig)) {
    const bool shouldApply = g_hasPendingConfig && !g_isTornDown;
    if (shouldApply) {
      ApplyConfig(hwnd, g_pendingConfig);
    }
    return;
  }
  // With the split off, the DLL stays loaded for theming alone.
  if (!SplitActive()) {
    if (wparam == CodeOf(Command::kApplyTheme)) {
      ApplyForCurrentOsMode(hwnd);
    } else if (wparam == CodeOf(Command::kTeardown)) {
      Teardown(hwnd);
    }
    return;
  }
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
    case Command::kDumpTabRows:
      tabrows::Dump(edge::McvForWindow(hwnd));
      return;
    case Command::kDumpColours:
      colour_dump::Dump(edge::McvForWindow(hwnd));
      return;
    case Command::kTraceColours:
      colour_dump::Trace(lparam != 0);
      return;
    case Command::kForceSkinTone:
      ForceSkinTone(hwnd, lparam);
      return;
    case Command::kForceCyberpunkSkin:
      ApplyCyberpunkSkin(true);
      NotifyColoursChanged(hwnd);
      return;
    case Command::kEndPreviewDetach:
      tabrows::SetDetachPreview(tabrows::kNoDetachPreview);
      return;
    case Command::kTeardown:
      Teardown(hwnd);
      return;
    case Command::kTabDrop:
      tabdrag::ApplyDrop();
      return;
    case Command::kGripMoveTo:
      tabdrag::SimulateGripMove(channel::PointOf(lparam));
      return;
    case Command::kGripRelease:
      tabdrag::SimulateGripEnd(edge155::off::TABDRAG_end_complete);
      return;
    case Command::kGripEscape:
      tabdrag::SimulateGripEnd(edge155::off::TABDRAG_end_cancel);
      return;
    case Command::kTabPress:
      tabdrag::SimulateTabPress(edge::McvForWindow(hwnd),
                                static_cast<int>(lparam));
      return;
    case Command::kHoldAlt:
      tabdrag::g_isScriptedAltHeld = lparam != 0;
      return;
    case Command::kLinkPane:
      links::OpenLinkPane();
      return;
    case Command::kCloseOfferPane:
      offer::CloseOfferPane();
      return;
    case Command::kApplyTheme:
      ApplyForCurrentOsMode(hwnd);
      return;
    default:
      break;
  }
  const bool isPreviewDetach = IsInRange(wparam, Command::kPreviewDetachBase,
                                         channel::kShowBarPaneLimit);
  if (isPreviewDetach) {
    drag::ShowDetachInTabList(edge::McvForWindow(hwnd),
                              OperandOf(wparam, Command::kPreviewDetachBase),
                              dock::Zone::kDetach);
    return;
  }
  const bool isGripPress =
      IsInRange(wparam, Command::kGripPressBase, channel::kShowBarPaneLimit);
  if (isGripPress) {
    tabdrag::SimulateGripPress(edge::McvForWindow(hwnd),
                               OperandOf(wparam, Command::kGripPressBase),
                               channel::PointOf(lparam));
    return;
  }
  const bool isShowBar =
      IsInRange(wparam, Command::kShowBarBase, channel::kShowBarPaneLimit);
  if (isShowBar) {
    ShowBarByCommand(hwnd, OperandOf(wparam, Command::kShowBarBase));
    return;
  }
  CreateOrGrowByCommand(hwnd, wparam);
}

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  // RegisterHotKey must run on the thread that owns the window; this WndProc is
  // exactly that thread, so register lazily on the first message we see — but
  // only while the split is on (the DLL may be loaded for theming alone).
  if (!g_hasHotKeys && SplitActive()) {
    RegisterHotKeys(hwnd);
  }
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
  const bool isFindPoll = message == WM_TIMER && wparam == find::kTimerId;
  if (isFindPoll) {
    find::Tick();
    return 0;
  }
  const bool isReadingPoll =
      message == WM_TIMER &&
      wparam == channel::TimerIdOf(channel::WindowTimer::kReadingPoll);
  if (isReadingPoll) {
    reading_mode::Poll();
    return 0;
  }
  const bool isRemixPoll =
      message == WM_TIMER &&
      wparam == channel::TimerIdOf(channel::WindowTimer::kRemixPoll);
  if (isRemixPoll) {
    remix_skin_mode::Poll();
    return 0;
  }
  const bool isRenderNudge = message == WM_TIMER && wparam == repaint::kTimerId;
  if (isRenderNudge) {
    split_ops::SyncPanesToMembers(edge::McvForWindow(hwnd));
    repaint::NudgeAll();
    return 0;
  }
  const bool isThemeRepaint =
      message == WM_TIMER && wparam == theme::kRepaintTimerId;
  if (isThemeRepaint) {
    theme::Repaint(hwnd);
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
    RunCommand(hwnd, wparam, lparam);
    return 0;
  }
  // Windows broadcasts WM_SETTINGCHANGE with "ImmersiveColorSet" when the user
  // flips light <-> dark. Re-apply the matching theme, but let Edge handle the
  // message first (it updates its own native theme), so defer via the channel.
  const bool isColorSchemeChange =
      message == WM_SETTINGCHANGE && lparam != 0 &&
      wcscmp(reinterpret_cast<const wchar_t*>(lparam), kColorSchemeSetting) ==
          0;
  if (isColorSchemeChange) {
    channel::Post(CodeOf(Command::kApplyTheme));
  }
  // The window the channel lives in is closing while Edge keeps others open:
  // without moving on, every later config, theme and script command was posted
  // to a dead window and the DLL went silent until Edge restarted.
  const bool isClosing = message == WM_NCDESTROY;
  if (isClosing) {
    WNDPROC originalProc = g_originalProc;
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(originalProc));
    anim::Remove();
    // Its hot-keys went with it; the next window registers them again.
    g_hasHotKeys = false;
    AdoptBrowserWindow(hwnd);
    return CallWindowProcW(originalProc, hwnd, message, wparam, lparam);
  }
  return CallWindowProcW(g_originalProc, hwnd, message, wparam, lparam);
}

// `excluded` (an LPARAM-packed HWND) is a window that is closing.
inline BOOL CALLBACK FindBrowserWindow(HWND hwnd, LPARAM excluded) {
  const bool isExcluded = hwnd == reinterpret_cast<HWND>(excluded);
  if (isExcluded) {
    return TRUE;
  }
  const bool isVisibleWithTitle =
      (IsWindowVisible(hwnd) != 0) && GetWindowTextLengthW(hwnd) != 0;
  if (!isVisibleWithTitle) {
    return TRUE;
  }
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  // Edge has other windows of this class (toasts, the theme-installed bubble,
  // drag images). Take only a real browser window — one with a live tab strip —
  // so the hot-keys and the theme apply reach it.
  const void* tabStrip = edge::TsmForWindow(hwnd);
  if (!tabStrip) {
    return TRUE;
  }
  channel::g_window = hwnd;
  Log("[control] found browser window hwnd=%p tsm=%p", hwnd, tabStrip);
  return FALSE;
}

// Subclasses a browser window other than `excluded` and points the channel at
// it; false when Edge has none open.
inline bool AdoptBrowserWindow(HWND excluded) {
  channel::g_window = nullptr;
  EnumWindows(FindBrowserWindow, reinterpret_cast<LPARAM>(excluded));
  HWND hwnd = channel::g_window;
  if (!hwnd) {
    Log("[control] no browser window to subclass");
    return false;
  }
  g_originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
      hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
  Log("[control] subclassed hwnd=%p msg=%u", hwnd, channel::g_message);
  anim::Install(hwnd);
  // Theming shares this DLL and its UI-thread window but is config-driven. Read
  // the panel's current choice from disk now (so a DLL injected into an open
  // Edge doesn't race the panel's WM_COPYDATA), and apply it on the UI thread.
  g_hasPendingConfig = LoadPendingConfig();
  if (g_hasPendingConfig) {
    channel::Post(CodeOf(Command::kApplyConfig));
  }
  // A (re)load into a split that is already grown: add the grips on the UI thread.
  channel::Post(CodeOf(Command::kEnsureGrips));
  return true;
}

inline void Install() {
  channel::g_message = RegisterWindowMessageW(channel::kMessageName);
  AdoptBrowserWindow(nullptr);
}

// Runs on the unload thread: hand the UI-side teardown to the UI thread and wait
// for it, then restore the original window procedure.
inline void Remove() {
  HWND hwnd = channel::g_window;
  const bool isSubclassed = (hwnd != nullptr) && (g_originalProc != nullptr);
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

}  // namespace edgeplus::control
