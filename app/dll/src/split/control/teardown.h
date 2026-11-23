// teardown.h — everything UI-side undone before the DLL unloads: grown splits
// broken up, every feature switched off and Edge's own look back, and no
// config apply still queued let to switch anything on again.
#pragma once
#include <windows.h>
#include "chrome/pane/dividers.h"
#include "chrome/pane/drag.h"
#include "chrome/pane/grip.h"
#include "chrome/pane/overlay.h"
#include "chrome/tabs/live_preview.h"
#include "chrome/tabs/tabdrag.h"
#include "find/find.h"
#include "layout/anim.h"
#include "permissions/protocol_handlers.h"
#include "split/control/appearance.h"
#include "split/control/hotkeys.h"
#include "split/control/split_commands.h"
#include "theme/follow/theme_follow.h"
#include "theme/reading/reading_mode.h"
#include "theme/remix/pages/remix_skin_mode.h"
#include "theme/remix/remix_mode.h"

namespace edgeplus::control {

// Set by the UI-side teardown before unload: from then on a config apply still
// queued behind it must not switch anything back on (it once turned the link
// handlers on mid-unload, leaving a vtable slot pointing into the freed DLL).
inline constinit bool g_isTornDown = false;

// Splits are broken up first, while the hooks still render the collapse.
inline void Teardown(HWND hwnd) {
  g_isTornDown = true;
  EnumWindows(BreakUpWindowSplits, 0);
  // On this thread, so no config apply can switch it back on behind us.
  protocol_handlers::Remove();
  if (reading_mode::g_isActive) {
    reading_mode::g_isActive = false;
    reading_mode::Stop(hwnd);
  }
  find::Apply(false);
  const bool wasFollowingTheme = theme_follow::g_isActive;
  theme_follow::g_isActive = false;
  if (wasFollowingTheme) {
    RefreshThemeColors();
  }
  const bool wasRemixFollowing = remix_mode::g_isActive;
  remix_mode::g_isActive = false;
  if (wasRemixFollowing) {
    remix_skin_mode::Stop(hwnd);
    RefreshThemeColors();
  }
  live_preview::g_isActive = false;
  live_preview::g_isCardOpen = false;
  drag::Cancel();
  tabdrag::Cancel();
  overlay::Remove();
  ApplyCyberpunkSkin(false);
  grip::RemoveAll();
  dividers::RemoveAll();
  anim::Remove();
  if (g_hasHotKeys) {
    UnregisterHotKeys(hwnd);
  }
  g_hasHotKeys = false;
}

}  // namespace edgeplus::control
