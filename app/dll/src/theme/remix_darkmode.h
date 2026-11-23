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
#include "core/config.h"
#include "core/handles.h"
#include "theme/remix_colors.h"
#include "theme/remix_colors_store.h"
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

// The Appearance template itself (the function the calls run), which Edge
// fetches and runs once per page, is kept for reading how it turns hue,
// saturation and darkmode into a theme: the first few scripts that name the
// theme function without calling it go to a file in the Edge+ data folder.
inline constexpr char kTemplateDumpPath[] = "\\EdgePlus\\remix-templates.txt";
inline constexpr int kTemplateDumpLimit = 6;
inline constinit int g_templatesDumped = 0;

inline void DumpTemplate(const std::string& script) {
  const bool isTemplate =
      script.find("applyColorWheelThemeV2") != std::string::npos ||
      script.find("__edgeRemixApplyTheme") != std::string::npos;
  if (!isTemplate || g_templatesDumped >= kTemplateDumpLimit) {
    return;
  }
  g_templatesDumped++;
  const std::string path = config::LocalAppDataPath(kTemplateDumpPath);
  const handles::UniqueHandle file{
      path.empty()
          ? INVALID_HANDLE_VALUE
          : CreateFileA(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!file) {
    return;
  }
  const std::string text = script + "\n=====\n";
  DWORD written = 0;
  WriteFile(file.Get(), text.data(), static_cast<DWORD>(text.size()), &written,
            nullptr);
  Log("[remix-theme] kept a template script (%zu chars)", script.size());
}

inline void RewriteScript(void* out) {
  const std::string script = edge::Utf16ToUtf8(edge::U16StringView(out));
  const bool isDark = g_edgeTone == theme_tone::Tone::kDark;
  remix_arguments::Result result =
      remix_arguments::WithDarkmode(script, isDark);
  if (result.outcome == remix_arguments::Outcome::kNotThemeCall) {
    DumpTemplate(script);
    return;
  }
  // The colour picked for Edge's current mode, when there is one; a remix with
  // none saved keeps its own.
  const remix_colors::Colour colour =
      remix_colors::ColourFor(remix_colors_store::g_pair, isDark);
  if (colour.isSet) {
    const bool isDarkmodeRewritten =
        result.outcome == remix_arguments::Outcome::kRewritten;
    const std::string current = isDarkmodeRewritten ? result.script : script;
    remix_arguments::Result coloured =
        remix_arguments::WithColour(current, colour.hue, colour.intensity);
    if (coloured.outcome == remix_arguments::Outcome::kRewritten) {
      result = std::move(coloured);
    }
  }
  Log("[remix-theme] appearance script (edge %s): outcome %d, %.*s",
      isDark ? "dark" : "light", static_cast<int>(result.outcome),
      static_cast<int>(std::min(script.size(), kLoggedScriptChars)),
      script.c_str());
  if (result.outcome != remix_arguments::Outcome::kRewritten) {
    return;
  }
  Log("[remix-theme] appearance script, as run: %.*s",
      static_cast<int>(std::min(result.script.size(), kLoggedScriptChars)),
      result.script.c_str());
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
