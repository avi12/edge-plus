// find_bar_session.h — the session a find bar belongs to, found from the
// bar's browser window, and a session reset or closed with its bar.
#pragma once
#include <windows.h>
#include "assistant/assistant_chat.h"
#include "core/channel.h"
#include "core/log.h"
#include "edge/api/browser.h"
#include "find/find_ask.h"
#include "find/find_poll.h"
#include "find/find_search.h"
#include "find/find_session.h"
#include "find/find_toggles.h"

namespace edgeplus::find {

inline void Reset(void* mcv, Session& session) {
  CancelAsk(mcv, session);
  ClearTarget(mcv, session);
  session.askedText.clear();
}

inline void Close(void* mcv) {
  Session* session = SessionFor(mcv);
  if (!session) {
    return;
  }
  Reset(mcv, *session);
  assistant_chat::Cancel(mcv);
  session->widenedBarWidth = 0;
  session->stockBarWidth = 0;
  session->findBarView = nullptr;
  StopTimerIfIdle();
  Log("[find] bar closed");
}

// Edge draws its find bar in a window of its own that also resolves to the
// browser's views, so a candidate must have a title (the bar's window has none):
// a session tied to the bar's window would be dropped the moment it changes.
inline bool IsBrowserWindow(HWND candidate) {
  const bool hasTitle = GetWindowTextLengthW(candidate) > 0;
  return (candidate != nullptr) && hasTitle &&
         (edge::McvForWindow(candidate) != nullptr);
}

inline HWND BrowserWindowInUse() {
  HWND foreground = GetForegroundWindow();
  HWND owner = GetAncestor(foreground, GA_ROOTOWNER);
  for (HWND candidate : {owner, foreground}) {
    if (IsBrowserWindow(candidate)) {
      return candidate;
    }
  }
  const bool isSubclassedUsable =
      (channel::g_window != nullptr) &&
      (edge::McvForWindow(channel::g_window) != nullptr);
  return isSubclassedUsable ? channel::g_window : nullptr;
}

inline Session* SessionOfBar(void* findBarView, void** mcvOut) {
  HWND window = BrowserWindowInUse();
  void* mcv = window ? edge::McvForWindow(window) : nullptr;
  if (!mcv) {
    return nullptr;
  }
  DropStale();
  Session& session = Sessions()[mcv];
  const bool isNewBar = session.findBarView != findBarView;
  if (isNewBar) {
    Reset(mcv, session);
    session.findBarView = findBarView;
    SyncToggles(findBarView);
  }
  session.window = window;
  *mcvOut = mcv;
  return &session;
}

}  // namespace edgeplus::find
