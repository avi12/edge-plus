// hotkeys.h — the split's hot-keys on a browser window: Ctrl+Shift+Alt with an
// arrow adds a pane on that side (A to the right), S swaps and X collapses;
// Shift+Alt with an arrow focuses the pane there.
#pragma once
#include <windows.h>
#include "core/log.h"
#include "edge/api/browser.h"
#include "layout/pane_tree.h"
#include "layout/state.h"
#include "split/split_data.h"
#include "split/split_ops.h"

namespace edgeplus::control {

inline constinit bool g_hasHotKeys = false;

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
  HotKey hotKey = HotKey::kAddPane;
  UINT modifiers = 0;
  UINT key = 0;
};

// Ctrl+Shift+Alt + an arrow adds a pane on that side; Shift+Alt + an arrow
// focuses the pane on that side.
inline constexpr UINT kEditModifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
inline constexpr UINT kFocusModifiers = MOD_SHIFT | MOD_ALT;
inline constexpr HotKeyBinding kHotKeyBindings[] = {
    {HotKey::kAddPane, kEditModifiers, 'A'},
    {HotKey::kCollapse, kEditModifiers, 'X'},
    {HotKey::kLeft, kEditModifiers, VK_LEFT},
    {HotKey::kUp, kEditModifiers, VK_UP},
    {HotKey::kRight, kEditModifiers, VK_RIGHT},
    {HotKey::kDown, kEditModifiers, VK_DOWN},
    {HotKey::kSwap, kEditModifiers, 'S'},
    {HotKey::kFocusLeft, kFocusModifiers, VK_LEFT},
    {HotKey::kFocusUp, kFocusModifiers, VK_UP},
    {HotKey::kFocusRight, kFocusModifiers, VK_RIGHT},
    {HotKey::kFocusDown, kFocusModifiers, VK_DOWN},
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

}  // namespace edgeplus::control
