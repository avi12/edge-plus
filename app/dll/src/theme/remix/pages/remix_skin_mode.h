// remix_skin_mode.h — puts the Cyberpunk skin on Create a remix's pages while
// both the remix switch and the skin are on, and takes it off when either goes
// (docs/re/remix-theme.md). A window timer runs the page script in every
// blueprint and remix page, as reading_mode.h does for its views: the bubble's
// page is not a tab, so a page opened later is painted a poll after it loads.
// UI thread.
#pragma once
#include <windows.h>
#include <string>
#include "chrome/tabs/tab_shape.h"
#include "core/channel.h"
#include "edge/edge.h"
#include "theme/remix/remix_colors.h"
#include "theme/remix/remix_colors_store.h"
#include "theme/remix/remix_darkmode.h"
#include "theme/remix/pages/remix_editor.h"
#include "theme/remix/remix_mode.h"
#include "theme/remix/pages/remix_selector.h"
#include "theme/remix/pages/remix_skin.h"
#include "theme/remix/pages/remix_wheel_bridge.h"
#include "theme/skin/skin_tone.h"

namespace edgeplus::remix_skin_mode {

// A page is handed the script on every beat: it is idempotent and cheap (a walk
// of a few shadow roots), and a page cannot be told from the last one at the
// same address, since a reload or a bubble reopened keeps its contents. The
// script keeps its own MutationObserver on every shadow root (remix_skin.h), so
// a component built later is styled before it is painted; the beat only has to
// find the page as soon as it is live.
inline constexpr UINT kPollIntervalMs = 50;

// True once the pages carry the skin, so the poll takes it off exactly once.
inline constinit bool g_isPainted = false;

// The page publishes the pair it holds in its address (remix_colors.h); a
// changed one is kept in the DLL's file, unless it holds no colour (a new
// appearance before the user picks one).
inline void SavePairOf(void* contents) {
  remix_colors::Pair published;
  const bool isPublished =
      remix_colors::PairFromUrl(edge::TabUrl(contents), &published);
  const bool shouldSave = isPublished && remix_colors::HasColour(published) &&
                          published != remix_colors_store::g_pair;
  if (shouldSave) {
    remix_colors_store::Save(published);
  }
}

inline void Poll() {
  if (!remix_mode::g_isActive) {
    return;
  }
  remix_darkmode::FollowModeFlip();
  const bool isSkinning = remix_skin::ShouldSkin(
      remix_mode::g_isActive, tab_shape::g_isCyberpunkSkinOn);
  const bool isRemovingSkin = !isSkinning && g_isPainted;
  std::string skinScript;
  if (isSkinning) {
    skinScript = remix_skin::PaintScript(skin_tone::Current());
  } else if (isRemovingSkin) {
    skinScript = remix_skin::RemoveScript();
  }
  // The colour selector needs only the remix switch; the skin just restyles it.
  const std::string selectorScript = remix_selector::SelectorScript(
      isSkinning, remix_colors::FormatPair(remix_colors_store::g_pair));
  const std::string wheelBridgeScript = remix_wheel_bridge::WheelBridgeScript();
  for (void* contents : remix_editor::RemixPages()) {
    if (!skinScript.empty()) {
      edge::RunScriptUnanswered(contents, skinScript);
    }
    // The bridge first: the selector script may ask it to set the wheel.
    edge::RunMainWorldScriptUnanswered(contents, wheelBridgeScript);
    edge::RunScriptUnanswered(contents, selectorScript);
    SavePairOf(contents);
  }
  g_isPainted = isSkinning;
}

inline void StartPolling(HWND window) {
  SetTimer(window, channel::TimerIdOf(channel::WindowTimer::kRemixPoll),
           kPollIntervalMs, nullptr);
}

inline void Stop(HWND window) {
  KillTimer(window, channel::TimerIdOf(channel::WindowTimer::kRemixPoll));
  const std::string script =
      remix_skin::RemoveScript() + remix_selector::SelectorRemoveScript();
  const std::string wheelBridgeRemoveScript =
      remix_wheel_bridge::WheelBridgeRemoveScript();
  for (void* contents : remix_editor::RemixPages()) {
    edge::RunScriptUnanswered(contents, script);
    edge::RunMainWorldScriptUnanswered(contents, wheelBridgeRemoveScript);
  }
  g_isPainted = false;
}

}  // namespace edgeplus::remix_skin_mode
