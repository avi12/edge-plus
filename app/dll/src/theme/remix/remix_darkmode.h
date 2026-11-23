// remix_darkmode.h — an Appearance remix takes the `darkmode` of its call from
// Edge's own light or dark, not the one baked in when it was made
// (docs/re/remix-theme.md). Every remix script reaches the page through one
// call; an Appearance call among them is rewritten here (remix_arguments.h)
// into a request to the page's own helper while the remix switch is on. UI
// thread.
#pragma once
#include <algorithm>
#include <string>
#include "MinHook.h"
#include "edge/edge.h"
#include "core/log.h"
#include "theme/follow/blink_scheme.h"
#include "theme/follow/followed_pages.h"
#include "theme/remix/remix_arguments.h"
#include "theme/remix/remix_colors.h"
#include "theme/remix/remix_colors_store.h"
#include "theme/remix/remix_follow_script.h"
#include "theme/remix/pages/remix_editor.h"
#include "theme/remix/remix_mode.h"
#include "theme/remix/remix_template_copy.h"
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

// Every remix script reaches the page through RenderFrameHostImpl::
// ExecuteJavaScriptInIsolatedWorld in the remix world: the editor's applies
// (built by RemixAction_BuildScript) and a stored remix applied as its page
// loads (built elsewhere, `(function(){try{NAME(...);return true;}catch(e)
// {return false;}})();`, which never passed the builder). So the rewrite sits
// there.
using ExecuteIsolated_t = void(__fastcall*)(const void*, const edge::U16String*,
                                            const void**, int);
inline constinit ExecuteIsolated_t o_execute_isolated = nullptr;

inline constexpr size_t kLoggedScriptChars = 120;
// What this DLL's own remix scripts carry, so they pass untouched.
inline constexpr std::string_view kOwnScriptMark = "__edgePlusRemix";
// Edge's Appearance calls are a line of script, while the template itself and
// Read Aloud's bundles, which run in the same world, are tens of kilobytes: a
// longer script is passed on without being read.
inline constexpr size_t kMaxThemeCallChars = 4096;

// `script` as the page is to run it, or empty when it is left as it is.
[[nodiscard]] inline std::string RewrittenScript(const std::string& script) {
  // The mode the page is to wear: the open Appearance editor's preview, else
  // Edge's own. Its colour comes from the pair the editor holds right now when
  // one is open (the DLL's copy is saved a poll later), else the saved pair; a
  // remix with that mode unset keeps its own, and one reset to no colour gets
  // saturation 0, which the helper wears as the page's own look. Then the
  // darkmode, relative to the page's own darkness (remix_arguments.h).
  const remix_editor::Editor editor = remix_editor::OpenEditor();
  const bool isPreviewing = editor.preview != theme_tone::Tone::kUnknown;
  const bool isTargetDark = isPreviewing
                                ? editor.preview == theme_tone::Tone::kDark
                                : g_edgeTone == theme_tone::Tone::kDark;
  const remix_colors::Pair& pair =
      editor.hasPair ? editor.pair : remix_colors_store::g_pair;
  std::string current = script;
  const remix_colors::Colour colour =
      remix_colors::ColourFor(pair, isTargetDark);
  if (colour.isSet) {
    remix_arguments::Result coloured =
        remix_arguments::WithColour(current, colour.hue, colour.intensity);
    if (coloured.outcome == remix_arguments::Outcome::kRewritten) {
      current = std::move(coloured.script);
    }
  }
  remix_arguments::Result result =
      remix_arguments::WithPageAwareTheme(current, isTargetDark, pair);
  if (result.outcome == remix_arguments::Outcome::kNotThemeCall) {
    return std::string();
  }
  remix_template_copy::KeepOnce();
  const bool isOnlyColoured =
      result.outcome != remix_arguments::Outcome::kRewritten &&
      current != script;
  if (isOnlyColoured) {
    result = {remix_arguments::Outcome::kRewritten, current};
  }
  Log("[remix-theme] appearance script (edge %s, target %s from %s%s): "
      "outcome %d, %.*s",
      g_edgeTone == theme_tone::Tone::kDark ? "dark" : "light",
      isTargetDark ? "dark" : "light", isPreviewing ? "the editor" : "edge",
      remix_colors::IsNoColour(colour) ? ", no colour" : "",
      static_cast<int>(result.outcome),
      static_cast<int>(std::min(script.size(), kLoggedScriptChars)),
      script.c_str());
  if (result.outcome != remix_arguments::Outcome::kRewritten) {
    return std::string();
  }
  Log("[remix-theme] appearance call, as run: %.*s",
      static_cast<int>(std::min(current.size(), kLoggedScriptChars)),
      current.c_str());
  return std::move(result.script);
}

// Edge's mode as the last push found it. When Edge flips, every open tab
// wearing an Appearance remix is applied again for the new mode, in the world
// remixes run in, whichever build applied it: Edge itself only re-runs a
// remix on the next load. UI thread, from the remix poll.
inline constinit theme_tone::Tone g_followedTone = theme_tone::Tone::kUnknown;

inline void FollowModeFlip() {
  const theme_tone::Tone tone = g_edgeTone;
  const bool isUnchanged =
      tone == theme_tone::Tone::kUnknown || tone == g_followedTone;
  if (isUnchanged) {
    return;
  }
  const bool isFirstSeen = g_followedTone == theme_tone::Tone::kUnknown;
  g_followedTone = tone;
  if (isFirstSeen) {
    return;
  }
  const bool isDark = tone == theme_tone::Tone::kDark;
  const std::string script =
      remix_follow_script::FollowModeScript(isDark, remix_colors_store::g_pair);
  int asked = 0;
  for (void* contents : edge::AllWebContents()) {
    const bool isTab = edge::IsPageLive(contents) &&
                       !followed_pages::IsRemixPage(edge::TabUrl(contents));
    const bool isAsked =
        isTab && edge::RunScriptUnanswered(contents, script,
                                           edge155::off::kRemixWorldId);
    if (isAsked) {
      asked++;
    }
  }
  Log("[remix-theme] edge went %s: %d page(s) asked to wear their remix in it",
      isDark ? "dark" : "light", asked);
}

// Every script any isolated world gets from the browser passes here, so a
// script for another world, or with the remix switch off, goes straight on.
// It counts itself (RunningDetour), so unload never frees it under a thread.
inline void __fastcall h_execute_isolated(const void* frame,
                                          const edge::U16String* script,
                                          const void** callback, int worldId) {
  const edge::RunningDetour running;
  const bool isRemixWorld = worldId == edge155::off::kRemixWorldId &&
                            remix_mode::g_isActive && script != nullptr;
  if (!isRemixWorld) {
    o_execute_isolated(frame, script, callback, worldId);
    return;
  }
  const edge::U16View scriptView = edge::U16StringView(script);
  const bool isCandidate = g_edgeTone != theme_tone::Tone::kUnknown &&
                           scriptView.length <= kMaxThemeCallChars;
  if (!isCandidate) {
    o_execute_isolated(frame, script, callback, worldId);
    return;
  }
  const std::string text = edge::Utf16ToUtf8(scriptView);
  const bool isOwnScript = text.find(kOwnScriptMark) != std::string::npos;
  const std::string rewritten =
      isOwnScript ? std::string() : RewrittenScript(text);
  if (rewritten.empty()) {
    o_execute_isolated(frame, script, callback, worldId);
    return;
  }
  edge::U16String fresh{};
  const edge::U8View rewrittenView{rewritten.data(), rewritten.size()};
  edge::Utf8ToUtf16(&fresh, &rewrittenView);
  o_execute_isolated(frame, &fresh, callback, worldId);
  edge::FreeU16String(&fresh);
}

// Its hook back on after the split's MH_ALL_HOOKS turned every hook off
// (control.h, with the split switched off).
inline void EnableAll() {
  edge::EnableHookAt(
      edge155::rva::RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld);
}

inline bool Install() {
  const uintptr_t site =
      edge155::rva::RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld;
  const bool isHooked =
      MH_CreateHook(edge::At(site), reinterpret_cast<void*>(h_execute_isolated),
                    reinterpret_cast<void**>(&o_execute_isolated)) == MH_OK;
  if (!isHooked) {
    Log("[err] remix-darkmode hook failed");
    return false;
  }
  const MH_STATUS status = edge::EnableHookAt(site);
  const bool isEnabled = status == MH_OK || status == MH_ERROR_ENABLED;
  Log("[ok] remix-darkmode hook installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

}  // namespace edgeplus::remix_darkmode
