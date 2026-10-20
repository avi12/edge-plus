// control.h — runs pane actions on Edge's UI thread. One concern: subclass the
// browser window so its hot-keys and the channel's message (channel.h, wParam =
// a Command) run pane actions on the very thread that owns the views (the
// WndProc runs on the UI thread). Hooks queue work there, and scripts post the
// same codes. The original WndProc is restored on unload.
#pragma once
#include <windows.h>
#include <cstring>
#include "MinHook.h"
#include "channel.h"
#include "config.h"
#include "edge.h"
#include "geometry.h"
#include "spine.h"
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
#include "protocol_handlers.h"
#include "tabdrag.h"
#include "tabrows.h"
#include "theme.h"
#include "theme_follow.h"
#include "live_preview.h"
#include "assistant.h"
#include "find.h"

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
  kFocusLeft = 8,
  kFocusUp = 9,
  kFocusRight = 10,
  kFocusDown = 11,
};

struct HotKeyBinding {
  HotKey hotKey;
  UINT modifiers;
  UINT key;
};

// Ctrl+Shift+Alt + a key reshapes the split (add / swap / collapse); Shift+Alt +
// an arrow moves focus between panes, matching the extension's pane focus. Keys
// are registered (and unregistered) in this order.
inline constexpr UINT kEditModifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
inline constexpr UINT kFocusModifiers = MOD_SHIFT | MOD_ALT;
inline constexpr HotKeyBinding kHotKeyBindings[] = {
    {HotKey::kAddPane, kEditModifiers, 'A'},     // add a pane to the right
    {HotKey::kCollapse, kEditModifiers, 'X'},    // collapse to active pane
    {HotKey::kLeft, kEditModifiers, VK_LEFT},    // add a pane to the left
    {HotKey::kUp, kEditModifiers, VK_UP},        // add a pane above
    {HotKey::kRight, kEditModifiers, VK_RIGHT},  // add a pane to the right
    {HotKey::kDown, kEditModifiers, VK_DOWN},    // add a pane below
    {HotKey::kSwap, kEditModifiers, 'S'},        // swap with the counterpart
    {HotKey::kFocusLeft, kFocusModifiers, VK_LEFT},    // focus the pane left
    {HotKey::kFocusUp, kFocusModifiers, VK_UP},        // focus the pane above
    {HotKey::kFocusRight, kFocusModifiers, VK_RIGHT},  // focus the pane right
    {HotKey::kFocusDown, kFocusModifiers, VK_DOWN},    // focus the pane below
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
    RegisterHotKey(hwnd, static_cast<int>(binding.hotKey), binding.modifiers,
                   binding.key);
  }
  Log("[control] hotkeys: Ctrl+Shift+Alt + arrows=add, A=add right, S=swap, "
      "X=collapse; Shift+Alt + arrows=focus a pane");
}

inline void UnregisterHotKeys(HWND hwnd) {
  for (const HotKeyBinding& binding : kHotKeyBindings) {
    UnregisterHotKey(hwnd, static_cast<int>(binding.hotKey));
  }
}

// Whether `hwnd` is a top-level window of this Edge process with the browser
// window class (it may still be a toast or bubble; callers check for a tab
// strip).
inline bool IsOurBrowserClassWindow(HWND hwnd) {
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  const bool isOurProcess = processId == GetCurrentProcessId();
  if (!isOurProcess) {
    return false;
  }
  wchar_t className[kClassNameLength] = {};
  GetClassNameW(hwnd, className, kClassNameLength);
  return wcscmp(className, kBrowserWindowClass) == 0;
}

// EnumWindows callback: break up every grown split in each of this process's
// browser windows (all on this UI thread), so none outlives the DLL. Switching
// the feature off leaves the user with plain tabs, not a split stock Edge would
// mis-render.
inline BOOL CALLBACK BreakUpWindowSplits(HWND hwnd, LPARAM /*unused*/) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* mcv = edge::McvForWindow(hwnd);
  void* tsm = mcv ? edge::McvTabStripModel(mcv) : nullptr;
  if (!tsm) {
    return TRUE;
  }
  const int dissolved = split_data::BreakUpGrownSplits(tsm);
  if (dissolved > 0) {
    // Nudge Edge to re-evaluate its split view now the active tab has left the
    // split, so the window starts collapsing before the hooks come off. The
    // grown MCV keeps surplus containers, so the final full-width layout may
    // still settle on the user's next interaction with the window.
    void* active = edge::ActiveTab(tsm);
    edge::MaybeUpdateSplitView(edge::McvBrowserView(mcv),
                               active ? edge::TabContents(active) : nullptr);
    ForgetTree(mcv);
  }
  // Every window, dissolved or not: a split collapsed earlier is in the same
  // state Edge's stock layout cannot take.
  geometry::RestoreStockVisibility(mcv);
  return TRUE;
}

// EnumWindows callback: have the followed pages open in each of this process's
// browser windows (theme_follow.h) take up a change of the switch. The count of
// pages refreshed so far is the int at `lparam`.
inline BOOL CALLBACK RefreshWindowFollowedPages(HWND hwnd, LPARAM lparam) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* tsm = edge::TsmForWindow(hwnd);
  if (tsm) {
    *reinterpret_cast<int*>(lparam) += theme_follow::RefreshPages(tsm);
  }
  return TRUE;
}

// EnumWindows callback: one spine tick (spine.h) for each of this process's
// browser windows, so every window marks its own selected tab.
inline BOOL CALLBACK TickWindowSpine(HWND hwnd, LPARAM /*unused*/) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  if (edge::TsmForWindow(hwnd)) {
    spine::TickWindow(hwnd);
  }
  return TRUE;
}

inline void TickSpines() {
  spine::BeginTick();
  EnumWindows(TickWindowSpine, 0);
}

inline void RefreshThemeColors() {
  int refreshed = 0;
  EnumWindows(RefreshWindowFollowedPages, reinterpret_cast<LPARAM>(&refreshed));
  Log("[theme-follow] %d open page(s) recomputed their colors", refreshed);
}

// Everything that must be undone on the UI thread before unload: first break
// every grown split back into standalone tabs (while the hooks still render the
// collapse), then end the drag session, the preview window, the grips, the
// animation timer and the hot-keys.
inline void Teardown(HWND hwnd) {
  EnumWindows(BreakUpWindowSplits, 0);
  find::Apply(false);
  const bool wasFollowingTheme = theme_follow::g_isActive;
  theme_follow::g_isActive = false;
  if (wasFollowingTheme) {
    RefreshThemeColors();
  }
  live_preview::g_isActive = false;
  live_preview::g_isCardOpen = false;
  drag::Cancel();
  tabdrag::Cancel();
  overlay::Remove();
  spine::Remove(hwnd);
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
    case HotKey::kFocusLeft:
      split_ops::FocusNeighbour(edge::McvForWindow(hwnd), Direction::kLeft);
      break;
    case HotKey::kFocusUp:
      split_ops::FocusNeighbour(edge::McvForWindow(hwnd), Direction::kUp);
      break;
    case HotKey::kFocusRight:
      split_ops::FocusNeighbour(edge::McvForWindow(hwnd), Direction::kRight);
      break;
    case HotKey::kFocusDown:
      split_ops::FocusNeighbour(edge::McvForWindow(hwnd), Direction::kDown);
      break;
  }
}

// --- the panel's live configuration ----------------------------------------

// Turn the split's hooks on or off without disturbing the window subclass that
// theming also rides. Disabling them leaves the menus and layout stock; the
// byte-patch safety fixes stay (they are inert without a grown split).
inline void SetSplitHooksEnabled(bool isEnabled) {
  if (isEnabled) {
    MH_EnableHook(MH_ALL_HOOKS);
  } else {
    MH_DisableHook(MH_ALL_HOOKS);
    // The theme garbage-collector suppressor is not a split hook: keep it on so
    // both assigned themes stay installed for live switching while the split is
    // off but theming is still on. (MH_ALL_HOOKS turned it off above.)
    MH_EnableHook(edge::At(edge155::rva::ThemeService_RemoveUnusedThemes));
    // Nor is the vertical-tab selected-tab recolour: it rides theming too.
    MH_EnableHook(edge::At(edge155::rva::BrowserThemePack_GetColor));
    // Nor is the selected tab's ink: theming too.
    MH_EnableHook(edge::At(edge155::rva::ColorProvider_GetColor));
    MH_EnableHook(edge::At(edge155::rva::EdgeTab_UpdateForegroundColors));
    // Nor are the link-handler detours (they pass through while that feature
    // is off).
    protocol_handlers::EnableHooks();
    // The chat button's chevron is its own feature, not a split hook: keep it
    // live.
    assistant::EnableAll();
    // Nor is the enhanced find: it follows its own switch.
    find::EnableAll();
    // Nor are the WebUI pages' theme colors.
    theme_follow::EnableAll();
    // Nor is the live tab preview.
    live_preview::EnableAll();
  }
}

// EnumWindows callback: lift the link-handler blocks of the profile behind each
// of this process's browser windows (protocol_handlers.h), adding the count to
// the size_t at `lparam`. A profile with no window open is not reached.
inline BOOL CALLBACK LiftWindowHandlerBlocks(HWND hwnd, LPARAM lparam) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* profile = edge::ProfileForTabStrip(edge::TsmForWindow(hwnd));
  *reinterpret_cast<size_t*>(lparam) +=
      protocol_handlers::LiftIgnoredHandlers(profile);
  return TRUE;
}

// Apply the panel's desired state (config.h): which features are on, and which
// theme to use for each OS mode. Read from the config file whenever the app
// signals a change (OnConfigChanged), so both take effect live.
inline void ApplyConfig(HWND hwnd, const config::Wire& wire) {
  const bool wasSplitActive = g_splitActive;
  g_splitActive = wire.splitActive != 0;
  const bool isTurningSplitOff = wasSplitActive && !g_splitActive;
  const bool isTurningSplitOn = !wasSplitActive && g_splitActive;
  if (isTurningSplitOff) {
    // Break grown splits up while the hooks still render the collapse, then
    // leave the menus and layout stock.
    EnumWindows(BreakUpWindowSplits, 0);
    SetSplitHooksEnabled(false);
    if (g_hasHotKeys) {
      UnregisterHotKeys(hwnd);
      g_hasHotKeys = false;
    }
  } else if (isTurningSplitOn) {
    SetSplitHooksEnabled(true);
    if (!g_hasHotKeys) {
      RegisterHotKeys(hwnd);
    }
  }
  theme::ApplyConfig(hwnd, wire);
  // The spine has its own switch, and needs theming: the squared corners take
  // the selected fill the theme's colour hook answers.
  const bool isSpineWanted = theme::g_isActive && wire.tabSpineActive != 0;
  if (isSpineWanted && !spine::g_isActive) {
    spine::Start(hwnd);
  } else if (!isSpineWanted && spine::g_isActive) {
    spine::Remove(hwnd);
  }
  const bool isFollowChanged = theme_follow::ApplyConfig(wire);
  if (isFollowChanged) {
    RefreshThemeColors();
  }
  live_preview::ApplyConfig(wire);
  const bool isTurningHandlersOn = protocol_handlers::ApplyConfig(wire);
  if (isTurningHandlersOn) {
    size_t liftedCount = 0;
    EnumWindows(LiftWindowHandlerBlocks,
                reinterpret_cast<LPARAM>(&liftedCount));
    Log("[handlers] lifted %zu blocked link handler(s)", liftedCount);
  }
  assistant::Apply(edge::McvForWindow(hwnd));
  const bool isFindActive = wire.enhancedFindActive != 0;
  find::Apply(isFindActive);
}

// The config read from disk at Install, applied on the UI thread via a posted
// kApplyConfig (ApplyConfig touches Edge views, so it must not run on the Init
// thread that does the subclassing).
inline config::Wire g_pendingConfig = {};
inline bool g_hasPendingConfig = false;

// The app signalled a config change (dllmain's waiter thread calls this): re-read
// the file and apply it on the UI thread. Posting to our own subclassed window
// means we never depend on the app guessing which window we hooked.
inline void OnConfigChanged() {
  g_hasPendingConfig = config::LoadFromFile(&g_pendingConfig);
  if (g_hasPendingConfig) {
    channel::Post(CodeOf(Command::kApplyConfig));
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
  void* mcv = edge::McvForWindow(hwnd);
  // A collapsed split keeps its surplus containers; they get no grips.
  const bool isGrownSplit =
      mcv && edge::McvActiveSplitPanes(mcv) > split_data::kMinSplitMembers;
  if (isGrownSplit) {
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
  // The config read from disk at load, now on the UI thread.
  if (wparam == CodeOf(Command::kRedrawFollowedPages)) {
    RefreshThemeColors();
    return;
  }
  if (wparam == CodeOf(Command::kReloadConfig)) {
    OnConfigChanged();
    return;
  }
  if (wparam == CodeOf(Command::kApplyConfig)) {
    if (g_hasPendingConfig) {
      ApplyConfig(hwnd, g_pendingConfig);
    }
    return;
  }
  // With the split switched off (the DLL stays loaded for theming), only the
  // theming apply and the unload teardown run; every split command is ignored.
  if (!SplitActive()) {
    if (wparam == CodeOf(Command::kApplyTheme)) {
      theme::ApplyForCurrentOsMode(hwnd);
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
    case Command::kApplyTheme:
      theme::ApplyForCurrentOsMode(hwnd);
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
  // exactly that thread, so register lazily on the first message we see — but
  // only while the split is on (the DLL may be loaded for theming alone).
  if (!g_hasHotKeys && SplitActive()) {
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
  const bool isFindPoll = message == WM_TIMER && wparam == find::kTimerId;
  if (isFindPoll) {
    find::Tick();
    return 0;
  }
  const bool isRenderNudge = message == WM_TIMER && wparam == repaint::kTimerId;
  if (isRenderNudge) {
    split_ops::SyncPanesToMembers(edge::McvForWindow(hwnd));
    repaint::NudgeAll();
    return 0;
  }
  const bool isSpineTick = message == WM_TIMER && wparam == spine::kTimerId;
  if (isSpineTick) {
    TickSpines();
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
    RunCommand(hwnd, wparam);
    return 0;
  }
  // Windows broadcasts WM_SETTINGCHANGE with "ImmersiveColorSet" when the user
  // flips light <-> dark. Re-apply the matching theme, but let Edge handle the
  // message first (it updates its own native theme), so defer via the channel.
  const bool isColorSchemeChange =
      message == WM_SETTINGCHANGE && lparam != 0 &&
      wcscmp(reinterpret_cast<const wchar_t*>(lparam), L"ImmersiveColorSet") ==
          0;
  if (isColorSchemeChange) {
    channel::Post(CodeOf(Command::kApplyTheme));
  }
  return CallWindowProcW(g_originalProc, hwnd, message, wparam, lparam);
}

// EnumWindows callback: claim the first visible, titled browser window of this
// process.
inline BOOL CALLBACK FindBrowserWindow(HWND hwnd, LPARAM /*unused*/) {
  const bool isVisibleWithTitle =
      IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) != 0;
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
  // Theming shares this DLL and its UI-thread window but is config-driven. Read
  // the panel's current choice from disk now (so a DLL injected into an open
  // Edge doesn't race the panel's WM_COPYDATA), and apply it on the UI thread.
  g_hasPendingConfig = config::LoadFromFile(&g_pendingConfig);
  if (g_hasPendingConfig) {
    channel::Post(CodeOf(Command::kApplyConfig));
  }
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
