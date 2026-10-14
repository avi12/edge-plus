// anim.h — animated pane transitions. One concern: when the tree's target rects
// change at a steady window size (add, collapse, swap), ease every pane from
// where it is on screen now to its target over a few frames, instead of jumping.
// The on-screen View bounds are the "from" (so a retarget mid-flight continues
// smoothly, and the native 2-pane layout hands off cleanly). A window timer
// re-runs the layout each frame; the layout detour asks Apply() for that frame's
// rects. A window resize snaps (it already tracks the pointer). Gated on the
// system's "Animation effects" setting. UI thread only.
#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include "edge.h"
#include "motion.h"
#include "pane_tree.h"

namespace fixedge::anim {

struct Flight {
  std::vector<LeafRect> from;  // parallel to `to`
  std::vector<LeafRect> to;
  ULONGLONG start = 0;
};
struct HostSize {
  int width = 0, height = 0;
};

inline bool g_enabled = false;  // SPI_GETCLIENTAREAANIMATION, read at Install
inline HWND g_hwnd = nullptr;   // the window whose timer drives the frames
inline constexpr UINT_PTR kTimerId = 0xF1ED;
inline constexpr UINT kFrameMs = 15;

inline std::map<void*, Flight>& Flights() {
  static std::map<void*, Flight> flights;
  return flights;
}
inline std::map<void*, HostSize>& Hosts() {
  static std::map<void*, HostSize> hosts;
  return hosts;
}

// Record the MCV's content size for this layout pass; true when it changed (a
// window resize), which snaps instead of animating. Called for every bounded
// pass, native ones included, so the first tiled pass knows the prior size.
inline bool NoteHost(void* mcv, int w, int h) {
  // A 0x0 pass (Edge probes/hides the view mid-resize) is not a size to compare
  // against later; snap it and keep the last real size.
  if (w <= 0 || h <= 0) {
    return true;
  }
  HostSize& host = Hosts()[mcv];
  const bool resized = host.width != 0 && (host.width != w || host.height != h);
  host = {w, h};
  return resized;
}

inline bool SameTargets(const std::vector<LeafRect>& a,
                        const std::vector<LeafRect>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].leaf != b[i].leaf || !motion::Same(a[i].rect, b[i].rect)) {
      return false;
    }
  }
  return true;
}

// Rewrite `placed` (each visible leaf's target rect) to this frame's rects.
// Starts a flight when the targets change, advances it otherwise, and ends it
// (targets pass through untouched) once the duration has elapsed or on `snap`.
inline void Apply(void* mcv, std::vector<LeafRect>& placed, bool snap) {
  auto it = Flights().find(mcv);
  if (!g_enabled || snap) {
    if (it != Flights().end()) {
      Flights().erase(it);
    }
    return;
  }
  if (it == Flights().end() || !SameTargets(it->second.to, placed)) {
    const int count = static_cast<int>(edge::McvCount(mcv));
    std::vector<Rect>
        before;  // every pane on screen now (hidden ones are empty)
    for (int i = 0; i < count; ++i) {
      before.push_back(edge::ViewBounds(edge::McvContainerAt(mcv, i)));
    }
    Flight f;
    f.to = placed;
    f.start = GetTickCount64();
    bool moving = false;
    for (const LeafRect& lr : placed) {
      const Rect now = before[lr.leaf];
      const Rect from =
          motion::Empty(now) ? motion::EntryRect(lr.rect, before) : now;
      f.from.push_back({lr.leaf, from});
      moving = moving || !motion::Same(from, lr.rect);
    }
    if (!moving) {
      if (it != Flights().end()) {
        Flights().erase(it);
      }
      return;
    }
    it = Flights().insert_or_assign(mcv, std::move(f)).first;
    SetTimer(g_hwnd, kTimerId, kFrameMs, nullptr);
    Log("[anim] flight start: %zu panes", placed.size());
  }
  const double p = static_cast<double>(GetTickCount64() - it->second.start) /
                   motion::kDurationMs;
  if (p >= 1.0) {
    Flights().erase(it);
    return;
  }
  const double t = motion::Ease(p);
  for (size_t i = 0; i < placed.size(); ++i) {
    placed[i].rect = motion::Lerp(it->second.from[i].rect, placed[i].rect, t);
  }
}

// WM_TIMER: re-run the layout of every MCV in flight; the detour calls Apply.
// The final pass lands exactly on the targets and ends the flight, after which
// the timer stops.
inline void Tick() {
  std::vector<void*> mcvs;
  for (const auto& [mcv, flight] : Flights()) {
    mcvs.push_back(mcv);
  }
  for (void* mcv : mcvs) {
    edge::InvalidateLayout(mcv);
    edge::LayoutImmediately(mcv);
  }
  if (Flights().empty()) {
    KillTimer(g_hwnd, kTimerId);
  }
}

inline void Install(HWND hwnd) {
  g_hwnd = hwnd;
  BOOL on = TRUE;
  SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0);
  g_enabled = on != FALSE;
  Log("[anim] transitions %s",
      g_enabled ? "on" : "off (system animation effects disabled)");
}

inline void Remove() {
  if (g_hwnd) {
    KillTimer(g_hwnd, kTimerId);
  }
  Flights().clear();
  Hosts().clear();
  g_hwnd = nullptr;
}

}  // namespace fixedge::anim
