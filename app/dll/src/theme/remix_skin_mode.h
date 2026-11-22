// remix_skin_mode.h — puts the Cyberpunk skin on Create a remix's pages while
// both the remix switch and the skin are on, and takes it off when either goes
// (docs/re/remix-theme.md). A window timer runs the page script in every
// blueprint and remix page, as reading_mode.h does for its views: the bubble's
// page is not a tab, so a page opened later is painted a poll after it loads.
// UI thread.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "chrome/tabs/tab_shape.h"
#include "core/channel.h"
#include "edge/edge.h"
#include "theme/followed_pages.h"
#include "theme/remix_mode.h"
#include "theme/remix_skin.h"
#include "theme/skin_tone.h"

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

[[nodiscard]] inline std::vector<void*> RemixPages() {
  std::vector<void*> pages;
  for (void* contents : edge::AllWebContents()) {
    const bool isRemixPage =
        edge::IsPageLive(contents) &&
        followed_pages::IsRemixPage(edge::TabUrl(contents));
    if (isRemixPage) {
      pages.push_back(contents);
    }
  }
  return pages;
}

inline void Poll() {
  if (!remix_mode::g_isActive) {
    return;
  }
  const bool isSkinning = remix_skin::ShouldSkin(
      remix_mode::g_isActive, tab_shape::g_isCyberpunkSkinOn);
  const bool isRemovingSkin = !isSkinning && g_isPainted;
  std::string skinScript;
  if (isSkinning) {
    skinScript = remix_skin::PaintScript(skin_tone::Current());
  } else if (isRemovingSkin) {
    skinScript = remix_skin::RemoveScript();
  }
  // The mode note needs only the remix switch; the skin just restyles it.
  const std::string noteScript = remix_skin::NoteScript(isSkinning);
  for (void* contents : RemixPages()) {
    if (!skinScript.empty()) {
      edge::RunScriptUnanswered(contents, skinScript);
    }
    edge::RunScriptUnanswered(contents, noteScript);
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
      remix_skin::RemoveScript() + remix_skin::NoteRemoveScript();
  for (void* contents : RemixPages()) {
    edge::RunScriptUnanswered(contents, script);
  }
  g_isPainted = false;
}

}  // namespace edgeplus::remix_skin_mode
