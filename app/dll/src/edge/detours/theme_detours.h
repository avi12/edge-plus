// theme_detours.h — the detours on Edge's ThemeService for theming: its
// collector of unused themes held off, no Undo bubble for a theme Edge+
// applies, and every return to the default theme reported with its cause.
#pragma once
#include <windows.h>
#include <cstdint>
#include "core/channel.h"
#include "core/log.h"
#include "core/wear_report.h"
#include "edge/api/strings.h"
#include "theme/theme.h"
#include "theme/theme_hold.h"
#include "theme/theme_notice.h"
#include "theme/theme_wear.h"

namespace edgeplus::hooks {

// ThemeService::RemoveUnusedThemes: Chromium's one-active-theme garbage
// collector. Theming keeps both the light and the dark theme installed so a mode
// flip is instant, and the GC would unload whichever is not active. Not a split
// hook: config_apply.h keeps it enabled with the split off.
using RemoveUnusedThemes_t = void(__fastcall*)(void*);
inline constinit RemoveUnusedThemes_t o_remove_unused_themes = nullptr;
inline void __fastcall h_remove_unused_themes(void* themeService) {
  if (theme::g_isActive) {
    return;
  }
  o_remove_unused_themes(themeService);
}

// ThemeService::OnThemeBuiltFromExtension(this, id, pack, suppress_infobar):
// asks for no "Installed theme ... Undo" bubble when the theme is one Edge+
// applies itself (theme_notice.h). A store install is any other id and passes
// through untouched, bubble and all. Rides theming, not the split
// (config_apply.h).
using OnThemeBuilt_t = void(__fastcall*)(void*, const void*, void*, bool);
inline constinit OnThemeBuilt_t o_on_theme_built = nullptr;
inline void __fastcall h_on_theme_built(void* themeService, const void* themeId,
                                        void* pack, bool isSuppressed) {
  const std::string idText = edge::StdStringText(themeId);
  const theme_notice::Assigned assigned{
      std::string_view(theme::g_lightTheme.value, theme::g_lightTheme.length),
      std::string_view(theme::g_darkTheme.value, theme::g_darkTheme.length),
      theme::g_isActive};
  const bool isKeptOff = theme_hold::ShouldKeepOff(
      std::string_view(theme::g_heldTheme.value, theme::g_heldTheme.length),
      idText, theme::g_ownSwitch.IsSettling(GetTickCount64()));
  if (isKeptOff) {
    // Not handed on, so the theme Edge just installed is never swapped in. Edge
    // has already recorded it as the current one, so what the mode wants is put
    // back (the apply runs on the UI thread, from the channel).
    Log("[theme] kept %s installed but off (added without Shift)",
        idText.c_str());
    channel::Post(channel::CodeOf(channel::Command::kApplyTheme));
    return;
  }
  const bool isSuppressedNow =
      theme_notice::ShouldSuppress(assigned, idText, isSuppressed);
  const bool isSuppressedByUs = isSuppressedNow && !isSuppressed;
  Log("[theme] built %s: bubble %s", idText.c_str(),
      isSuppressedByUs ? "suppressed (Edge+ theme)" : "left to Edge");
  o_on_theme_built(themeService, themeId, pack, isSuppressedNow);
  theme::g_ownSwitch.Built(idText);
  wear_report::Report(idText, theme_wear::CauseOfTheme(assigned, idText));
}

// ThemeService::UseDefaultTheme(this at its +0x10 base, ...): every return to
// Edge's default theme, Edge's own "Restore default" included, ends here. The
// second register is passed through untouched (its meaning is not needed).
// Reported with who caused it (theme_wear.h), so the panel can follow the
// user's choice; the skin's colours from the last theme pack go stale either way.
using UseDefaultTheme_t = void(__fastcall*)(void*, uintptr_t);
inline constinit UseDefaultTheme_t o_use_default_theme = nullptr;
inline void __fastcall h_use_default_theme(void* themeServiceBase,
                                           uintptr_t argument) {
  o_use_default_theme(themeServiceBase, argument);
  theme::g_isDefaultThemeApplied = true;
  const theme_wear::Cause cause = theme_wear::CauseOfDefault(
      theme::g_isUsingDefaultOurselves,
      theme::g_ownSwitch.IsSettling(GetTickCount64()));
  Log("[theme] default theme worn (%s)",
      cause == theme_wear::Cause::kUser ? "the user's choice" : "Edge+");
  wear_report::Report({}, cause);
}

}  // namespace edgeplus::hooks
