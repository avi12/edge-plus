// remix_darkmode.h — an Appearance remix takes the `darkmode` of its call from
// Edge's own light or dark, not the one baked in when it was made
// (docs/re/remix-theme.md). Edge builds the script text of every remix it runs
// in one function; its result is rewritten here (remix_arguments.h) while the
// remix switch is on. UI thread.
#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include "MinHook.h"
#include "edge/edge.h"
#include "core/log.h"
#include "theme/blink_scheme.h"
#include "theme/followed_pages.h"
#include "theme/remix_arguments.h"
#include "theme/remix_mode.h"
#include "theme/theme_tone.h"

namespace edgeplus::remix_darkmode {

// The mode of the page whose web preferences Edge computed last: the script
// runs for a page Edge has just navigated, so that is the profile's mode.
inline constinit theme_tone::Tone g_edgeTone = theme_tone::Tone::kUnknown;

// Edge's own light or dark for the page behind `webContents`: its theme's tone
// (the same signal the followed pages use), else the scheme Edge computed for
// the page, which already includes the Appearance setting.
// Called by theme_follow's detour right after Edge's own answer, `preferred`
// being that answer. Remix pages are skipped: the follow overrides theirs.
inline void ObservePage(void* webContents, const void* preferred,
                        const followed_pages::Switches& switches) {
  const bool isWanted = remix_mode::g_isActive && webContents != nullptr;
  if (!isWanted) {
    return;
  }
  const bool isFollowedPage =
      followed_pages::Follows(edge::VisibleUrl(webContents), switches);
  if (isFollowedPage) {
    return;
  }
  void* profile = edge::BrowserContextOf(webContents);
  void* themeService =
      profile ? edge::ThemeServiceForProfile(profile) : nullptr;
  theme_tone::Tone tone =
      themeService ? edge::ThemeTone(themeService) : theme_tone::Tone::kUnknown;
  if (tone == theme_tone::Tone::kUnknown) {
    const bool isOwnDark = blink_scheme::IsDarkPageScheme(edge::Field<int32_t>(
        preferred, edge155::off::PREFERREDSCHEME_color_scheme));
    tone = isOwnDark ? theme_tone::Tone::kDark : theme_tone::Tone::kLight;
  }
  g_edgeTone = tone;
}

// The remix script builder: out (a std::u16string, returned) <- the script of
// the action `action` points to.
using BuildScript_t = void*(__fastcall*)(void*, void*);
inline constinit BuildScript_t o_build_script = nullptr;

inline constexpr size_t kLoggedScriptChars = 120;

inline void RewriteScript(void* out) {
  const std::string script = edge::Utf16ToUtf8(edge::U16StringView(out));
  const bool isDark = g_edgeTone == theme_tone::Tone::kDark;
  remix_arguments::Result result =
      remix_arguments::WithDarkmode(script, isDark);
  if (result.outcome == remix_arguments::Outcome::kNotThemeCall) {
    return;
  }
  Log("[remix-theme] appearance script (edge %s): outcome %d, %.*s",
      isDark ? "dark" : "light", static_cast<int>(result.outcome),
      static_cast<int>(std::min(script.size(), kLoggedScriptChars)),
      script.c_str());
  if (result.outcome != remix_arguments::Outcome::kRewritten) {
    return;
  }
  edge::U16String fresh{};
  const edge::U8View view{result.script.data(), result.script.size()};
  edge::Utf8ToUtf16(&fresh, &view);
  edge::FreeU16String(static_cast<edge::U16String*>(out));
  std::memcpy(out, &fresh, sizeof(fresh));
}

inline void* __fastcall h_build_script(void* out, void* action) {
  void* result = o_build_script(out, action);
  const bool isKnown = g_edgeTone != theme_tone::Tone::kUnknown;
  const bool shouldRewrite = remix_mode::g_isActive && isKnown;
  if (shouldRewrite) {
    RewriteScript(out);
  }
  return result;
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(edge::At(edge155::rva::RemixAction_BuildScript),
                    reinterpret_cast<void*>(h_build_script),
                    reinterpret_cast<void**>(&o_build_script)) == MH_OK;
  if (!isHooked) {
    Log("[err] remix-darkmode hook failed");
    return false;
  }
  const MH_STATUS status =
      edge::EnableHookAt(edge155::rva::RemixAction_BuildScript);
  const bool isEnabled = status == MH_OK || status == MH_ERROR_ENABLED;
  Log("[ok] remix-darkmode hook installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

}  // namespace edgeplus::remix_darkmode
