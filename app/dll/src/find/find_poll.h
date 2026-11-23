// find_poll.h — the timer that polls a question while the assistant answers
// (it turns the spinners and keeps the bar wide), started with the first and
// stopped once no session is asking.
#pragma once
#include <windows.h>
#include "assistant/assistant_chat.h"
#include "core/channel.h"
#include "find/find_session.h"

namespace edgeplus::find {

inline constexpr UINT_PTR kTimerId =
    channel::TimerIdOf(channel::WindowTimer::kFindPoll);
inline constexpr UINT kTickIntervalMs = 100;

inline void StartTimer() {
  if (channel::g_window) {
    SetTimer(channel::g_window, kTimerId, kTickIntervalMs, nullptr);
  }
}

inline bool IsAnyAsking() {
  for (const auto& [mcv, session] : Sessions()) {
    const bool isHoldingWidth =
        (session.findBarView != nullptr) && session.widenedBarWidth != 0;
    if (session.ask.id != 0 || isHoldingWidth) {
      return true;
    }
  }
  return assistant_chat::IsAnyStopping();
}

inline void StopTimerIfIdle() {
  if (!IsAnyAsking() && channel::g_window) {
    KillTimer(channel::g_window, kTimerId);
  }
}

}  // namespace edgeplus::find
