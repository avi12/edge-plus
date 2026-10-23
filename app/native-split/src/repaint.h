// repaint.h — making panes draw after a move. A pane whose page was resized or
// reparented by a drag shows nothing until its page is resized again or the
// window changes focus, because its compositor has no frame to draw. So every
// visible pane is resized one pixel and back inside a single UI task, so
// nothing paints in between. UI thread only.
#pragma once
#include <windows.h>
#include "channel.h"
#include "edge.h"
#include "motion.h"

namespace fixedge::repaint {

inline constexpr UINT_PTR kTimerId =
    channel::TimerIdOf(channel::WindowTimer::kRenderNudge);
// After the pane transition has landed (it runs motion::kDurationMs).
inline constexpr UINT kSettleMarginMs = 120;
inline constexpr int kNudgePixels = 1;

inline void ScheduleAll() {
  SetTimer(channel::g_window, kTimerId,
           static_cast<UINT>(motion::kDurationMs) + kSettleMarginMs, nullptr);
}

inline void NudgeAll() {
  KillTimer(channel::g_window, kTimerId);
  void* mcv = edge::McvForWindow(channel::g_window);
  if (!mcv) {
    return;
  }
  const int count = static_cast<int>(edge::McvCount(mcv));
  int nudged = 0;
  for (int i = 0; i < count; i++) {
    void* container = edge::McvContainerAt(mcv, i);
    const bool isOnScreen = edge::ViewGetVisible(container);
    if (!isOnScreen) {
      continue;
    }
    // Edge hides the tab that was active before a switch after our feeder has
    // already shown it, leaving a pane that is on screen but hidden: it draws
    // nothing until the window loses focus and Edge recomputes visibility.
    void* contents = edge::ContainerWebContents(container);
    if (contents) {
      edge::WebContentsWasShown(contents);
    }
    const Rect bounds = edge::ViewBounds(container);
    Rect smaller = bounds;
    smaller.width -= kNudgePixels;
    edge::SetViewBounds(container, smaller);
    edge::LayoutImmediately(container);
    edge::SetViewBounds(container, bounds);
    edge::LayoutImmediately(container);
    nudged++;
  }
  Log("[repaint] nudged %d pane(s) to paint", nudged);
}

}  // namespace fixedge::repaint
