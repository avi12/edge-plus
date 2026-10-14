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
  int width = 0;
  int height = 0;
};

inline bool g_isEnabled = false;  // SPI_GETCLIENTAREAANIMATION, read at Install
inline HWND g_hwnd = nullptr;     // the window whose timer drives the frames
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
inline bool NoteHost(void* mcv, int width, int height) {
  // A 0x0 pass (Edge probes/hides the view mid-resize) is not a size to compare
  // against later; snap it and keep the last real size.
  const bool hasSize = width > 0 && height > 0;
  if (!hasSize) {
    return true;
  }
  HostSize& host = Hosts()[mcv];
  const bool isResized =
      host.width != 0 && (host.width != width || host.height != height);
  host = {width, height};
  return isResized;
}

inline bool SameTargets(const std::vector<LeafRect>& targets,
                        const std::vector<LeafRect>& otherTargets) {
  const bool isSameCount = targets.size() == otherTargets.size();
  if (!isSameCount) {
    return false;
  }
  for (size_t i = 0; i < targets.size(); i++) {
    const bool isSameTarget =
        targets[i].leaf == otherTargets[i].leaf &&
        motion::Same(targets[i].rect, otherTargets[i].rect);
    if (!isSameTarget) {
      return false;
    }
  }
  return true;
}

// Rewrite `placed` (each visible leaf's target rect) to this frame's rects.
// Starts a flight when the targets change, advances it otherwise, and ends it
// (targets pass through untouched) once the duration has elapsed or on
// `shouldSnap`.
inline void Apply(void* mcv, std::vector<LeafRect>& placed, bool shouldSnap) {
  auto flight = Flights().find(mcv);
  const bool hasFlight = flight != Flights().end();
  const bool shouldAnimate = g_isEnabled && !shouldSnap;
  if (!shouldAnimate) {
    if (hasFlight) {
      Flights().erase(flight);
    }
    return;
  }
  const bool isFlyingToTargets =
      hasFlight && SameTargets(flight->second.to, placed);
  if (!isFlyingToTargets) {
    const int count = static_cast<int>(edge::McvCount(mcv));
    // Every pane on screen now (hidden ones are empty).
    std::vector<Rect> before;
    before.reserve(count);
    for (int i = 0; i < count; i++) {
      before.push_back(edge::ViewBounds(edge::McvContainerAt(mcv, i)));
    }
    Flight newFlight;
    newFlight.to = placed;
    newFlight.start = GetTickCount64();
    bool isMoving = false;
    for (const LeafRect& target : placed) {
      const Rect onScreen = before[target.leaf];
      const Rect from = motion::Empty(onScreen)
                            ? motion::EntryRect(target.rect, before)
                            : onScreen;
      newFlight.from.push_back({target.leaf, from});
      isMoving = isMoving || !motion::Same(from, target.rect);
    }
    if (!isMoving) {
      if (hasFlight) {
        Flights().erase(flight);
      }
      return;
    }
    flight = Flights().insert_or_assign(mcv, std::move(newFlight)).first;
    SetTimer(g_hwnd, kTimerId, kFrameMs, nullptr);
    Log("[anim] flight start: %zu panes", placed.size());
  }
  const double progress =
      static_cast<double>(GetTickCount64() - flight->second.start) /
      motion::kDurationMs;
  const bool hasLanded = progress >= 1.0;
  if (hasLanded) {
    Flights().erase(flight);
    return;
  }
  const double eased = motion::Ease(progress);
  for (size_t i = 0; i < placed.size(); i++) {
    placed[i].rect =
        motion::Lerp(flight->second.from[i].rect, placed[i].rect, eased);
  }
}

// WM_TIMER: re-run the layout of every MCV in flight; the detour calls Apply.
// The final pass lands exactly on the targets and ends the flight, after which
// the timer stops. The MCVs are copied out first because Apply erases a landed
// flight mid-loop.
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
  BOOL isAnimationEnabled = TRUE;
  SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &isAnimationEnabled, 0);
  g_isEnabled = isAnimationEnabled != FALSE;
  Log("[anim] transitions %s",
      g_isEnabled ? "on" : "off (system animation effects disabled)");
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
