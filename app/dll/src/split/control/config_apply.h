// config_apply.h — the panel's choice applied on the UI thread, minus every
// feature this Edge build stood down: the split with its hooks and hot-keys,
// the reading and remix themes, the protocol handlers and the rest.
#pragma once
#include <windows.h>
#include "MinHook.h"
#include "assistant/assistant.h"
#include "chrome/tabs/live_preview.h"
#include "core/config.h"
#include "core/log.h"
#include "edge/api/binding.h"
#include "edge/api/browser.h"
#include "edge/availability.h"
#include "edge/offsets.h"
#include "find/find.h"
#include "layout/state.h"
#include "permissions/protocol_handlers.h"
#include "split/control/appearance.h"
#include "split/control/browser_windows.h"
#include "split/control/hotkeys.h"
#include "split/control/split_commands.h"
#include "theme/follow/theme_follow.h"
#include "theme/reading/reading_mode.h"
#include "theme/remix/pages/remix_skin_mode.h"
#include "theme/remix/remix_darkmode.h"
#include "theme/remix/remix_mode.h"
#include "theme/theme.h"

namespace edgeplus::control {

// Turn the split's hooks on or off without disturbing the window subclass that
// theming also rides. Disabling them leaves the menus and layout stock; the
// byte-patch safety fixes stay (they are inert without a grown split).
inline void SetSplitHooksEnabled(bool isEnabled) {
  if (isEnabled) {
    MH_EnableHook(MH_ALL_HOOKS);
  } else {
    MH_DisableHook(MH_ALL_HOOKS);
    // MH_ALL_HOOKS also turned off the hooks that are not the split's: theming's
    // (the theme garbage-collector suppressor keeps both assigned themes
    // installed for live switching) and those of features with their own switch.
    edge::EnableHookAt(edge155::rva::ThemeService_RemoveUnusedThemes);
    edge::EnableHookAt(edge155::rva::ThemeService_OnThemeBuiltFromExtension);
    edge::EnableHookAt(edge155::rva::ThemeService_UseDefaultTheme);
    edge::EnableHookAt(edge155::rva::BrowserThemePack_GetColor);
    edge::EnableHookAt(edge155::rva::ColorProvider_GetColor);
    edge::EnableHookAt(edge155::rva::EdgeTab_UpdateForegroundColors);
    // The Cyberpunk skin's tab outline: its own switch, not the split's.
    edge::EnableHookAt(edge155::rva::EdgeTabStyleViews_GetPath);
    edge::EnableHookAt(edge155::rva::SkPath_arcTo);
    edge::EnableHookAt(edge155::rva::SkRRect_setRectXY);
    edge::EnableHookAt(edge155::rva::EdgeTabStyleViews_IsLeftSplitTab);
    edge::EnableHookAt(edge155::rva::EdgeTabStyleViews_IsRightSplitTab);
    edge::EnableHookAt(edge155::rva::Canvas_DrawRoundRect);
    // Its tab frames (tab_frame.h).
    edge::EnableHookAt(edge155::rva::EdgeTabStyleViews_PaintBackgroundStroke);
    edge::EnableHookAt(edge155::rva::TabStrip_GetStrokeThickness);
    // Its title font (tab_font.h).
    edge::EnableHookAt(edge155::rva::EdgeTabStyleViews_PaintTab);
    // Its omnibox frame (omnibox_frame.h).
    edge::EnableHookAt(
        edge155::rva::LocationBarView_EdgeCreateRoundRectBackground);
    protocol_handlers::EnableHooks();
    find::EnableAll();
    theme_follow::EnableAll();
    remix_darkmode::EnableAll();
    live_preview::EnableAll();
  }
}

// Adds the count to the size_t at `lparam`. A profile with no window open is not
// reached.
inline BOOL CALLBACK LiftWindowHandlerBlocks(HWND hwnd, LPARAM lparam) {
  if (!IsOurBrowserClassWindow(hwnd)) {
    return TRUE;
  }
  void* profile = edge::ProfileForTabStrip(edge::TsmForWindow(hwnd));
  *reinterpret_cast<size_t*>(lparam) +=
      protocol_handlers::LiftIgnoredHandlers(profile);
  return TRUE;
}

inline void ApplySplitConfig(HWND hwnd, const config::Wire& wire) {
  const bool wasSplitActive = g_splitActive;
  g_splitActive = wire.splitActive != 0;
  const bool isTurningSplitOff = wasSplitActive && !g_splitActive;
  const bool isTurningSplitOn = !wasSplitActive && g_splitActive;
  if (isTurningSplitOff) {
    // Break splits up while the hooks still render the collapse.
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
}

inline void ApplyReadingConfig(HWND hwnd, const config::Wire& wire) {
  const bool wasReadingActive = reading_mode::g_isActive;
  const bool isReadingTurningOn = reading_mode::ApplyConfig(wire);
  if (isReadingTurningOn) {
    EnumWindows(ApplyWindowReadingTheme, 0);
    reading_mode::StartPolling(hwnd);
  } else if (wasReadingActive && !reading_mode::g_isActive) {
    reading_mode::Stop(hwnd);
  }
}

inline void ApplyRemixConfig(HWND hwnd, const config::Wire& wire) {
  const bool isRemixChanged = remix_mode::ApplyConfig(wire);
  if (!isRemixChanged) {
    return;
  }
  RefreshThemeColors();
  if (remix_mode::g_isActive) {
    remix_skin_mode::StartPolling(hwnd);
  } else {
    remix_skin_mode::Stop(hwnd);
  }
}

inline void ApplyHandlersConfig(const config::Wire& wire) {
  const bool isTurningHandlersOn = protocol_handlers::ApplyConfig(wire);
  if (isTurningHandlersOn) {
    size_t liftedCount = 0;
    EnumWindows(LiftWindowHandlerBlocks,
                reinterpret_cast<LPARAM>(&liftedCount));
    Log("[handlers] lifted %zu blocked link handler(s)", liftedCount);
  }
}

// A feature this Edge build stood down (availability.h) is never applied, not
// even as "off": it was never on in this process, so there is nothing to undo,
// and undoing would call into code it could not place.
inline void ApplyConfig(HWND hwnd, const config::Wire& wire) {
  using edge155::signatures::Feature;
  if (availability::IsAvailable(Feature::kSplit)) {
    ApplySplitConfig(hwnd, wire);
  }
  const bool isSkinAvailable =
      availability::IsAvailable(Feature::kCyberpunkSkin) &&
      availability::IsAvailable(Feature::kTheming);
  if (isSkinAvailable) {
    // The skin has its own switch, and needs theming: its colours are read
    // with the theme's (colour_detours.h h_getcolor). It is set first, so the
    // theme's re-apply below rebuilds the tab colours with the skin on or off.
    const bool isSkinOn =
        wire.themingActive != 0 && wire.cyberpunkSkinActive != 0;
    ApplyCyberpunkSkin(isSkinOn);
  }
  if (availability::IsAvailable(Feature::kTheming)) {
    theme::ApplyConfig(hwnd, wire);
  }
  if (availability::IsAvailable(Feature::kWebUiFollowsTheme)) {
    const bool isFollowChanged = theme_follow::ApplyConfig(wire);
    if (isFollowChanged) {
      RefreshThemeColors();
    }
  }
  if (availability::IsAvailable(Feature::kRemixTheme)) {
    ApplyRemixConfig(hwnd, wire);
  }
  if (availability::IsAvailable(Feature::kLivePreview)) {
    live_preview::ApplyConfig(wire);
  }
  if (availability::IsAvailable(Feature::kReadingTheme)) {
    ApplyReadingConfig(hwnd, wire);
  }
  if (availability::IsAvailable(Feature::kProtocolHandlers)) {
    ApplyHandlersConfig(wire);
  }
  if (availability::IsAvailable(Feature::kEnhancedFind)) {
    assistant::RemoveChevron(edge::McvForWindow(hwnd));
    const bool isFindActive = wire.enhancedFindActive != 0;
    find::Apply(isFindActive);
  }
}

// Applied through a posted kApplyConfig: ApplyConfig touches Edge views, so it
// must not run on the Init thread that does the subclassing.
inline constinit config::Wire g_pendingConfig = {};
inline constinit bool g_hasPendingConfig = false;

// The panel's choice from disk, minus every feature this Edge build cannot run
// (availability.h).
inline bool LoadPendingConfig() {
  const bool isLoaded = config::LoadFromFile(&g_pendingConfig);
  if (isLoaded) {
    availability::Mask(&g_pendingConfig);
  }
  return isLoaded;
}

}  // namespace edgeplus::control
