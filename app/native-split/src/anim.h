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
#include <set>
#include <vector>
#include "edge.h"
#include "motion.h"
#include "pane_tree.h"

namespace fixedge::anim {

struct Flight {
  std::vector<LeafRect> from;  // parallel to `to`
  std::vector<LeafRect> to;
  std::vector<const void*> contents;  // the tab each placed container showed
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

// Set before a pane is added that must paint at once (a tab dropped in): its
// page then takes its final size in one step instead of being resized every
// frame while it grows in, which left it blank until the growth finished. The
// next flight's new panes appear in place; the flag clears when that flight is
// planned.
inline bool g_isNextEntryInPlace = false;

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

// Splits being resized live (a divider drag): their layout follows the
// pointer exactly, with no easing.
inline std::set<void*>& LiveSplits() {
  static std::set<void*> splits;
  return splits;
}
inline void SetLive(void* mcv, bool isLive) {
  if (isLive) {
    LiveSplits().insert(mcv);
  } else {
    LiveSplits().erase(mcv);
  }
}

// Rewrite `placed` (each visible leaf's target rect) to this frame's rects.
// Starts a flight when the targets change, advances it otherwise, and ends it
// (targets pass through untouched) once the duration has elapsed or on
// `shouldSnap`.
// Where each tab (by the WebContents it shows) was last drawn: where its next
// move starts, even when Edge re-feeds it into a different container (closing a
// pane shifts every later tab one container earlier).
inline std::map<const void*, Rect>& LastDrawn() {
  static std::map<const void*, Rect> rects;
  return rects;
}
inline void ForgetTab(const void* contents) {
  LastDrawn().erase(contents);
}

inline std::vector<const void*> ContentsOf(
    void* mcv, const std::vector<LeafRect>& placed) {
  std::vector<const void*> contents;
  contents.reserve(placed.size());
  for (const LeafRect& leafRect : placed) {
    contents.push_back(
        edge::ContainerWebContents(edge::McvContainerAt(mcv, leafRect.leaf)));
  }
  return contents;
}

inline void Remember(const std::vector<const void*>& contents,
                     const std::vector<LeafRect>& placed) {
  for (size_t i = 0; i < placed.size(); i++) {
    if (contents[i]) {
      LastDrawn()[contents[i]] = placed[i].rect;
    }
  }
}

// Rewrite `placed` (each visible leaf's target rect) to this frame's rects.
// A flight starts when the targets change or a container now shows a different
// tab; each pane starts where its tab was last drawn, else, in a container new
// to the screen, grows in from the pane it was carved out of, else appears in
// place. A flight ends when its duration has elapsed, or at once on
// `shouldSnap` or a live resize.
inline void Apply(void* mcv, std::vector<LeafRect>& placed, bool shouldSnap) {
  const std::vector<const void*> contents = ContentsOf(mcv, placed);
  auto flight = Flights().find(mcv);
  const bool hasFlight = flight != Flights().end();
  const bool shouldAnimate =
      g_isEnabled && !shouldSnap && !LiveSplits().count(mcv);
  if (!shouldAnimate) {
    if (hasFlight) {
      Flights().erase(flight);
    }
    Remember(contents, placed);
    return;
  }
  const bool isFlyingToTargets = hasFlight &&
                                 SameTargets(flight->second.to, placed) &&
                                 flight->second.contents == contents;
  if (!isFlyingToTargets) {
    const int count = static_cast<int>(edge::McvCount(mcv));
    // Every pane on screen now. A hidden container counts as empty: it keeps
    // the bounds it last showed (a layout sets only visible children's).
    std::vector<Rect> before;
    before.reserve(count);
    for (int i = 0; i < count; i++) {
      void* container = edge::McvContainerAt(mcv, i);
      before.push_back(edge::ViewGetVisible(container)
                           ? edge::ViewBounds(container)
                           : Rect{});
    }
    Flight newFlight;
    newFlight.to = placed;
    newFlight.contents = contents;
    newFlight.start = GetTickCount64();
    bool isMoving = false;
    for (size_t i = 0; i < placed.size(); i++) {
      const LeafRect& target = placed[i];
      const auto lastDrawn = LastDrawn().find(contents[i]);
      const Rect onScreen = before[target.leaf];
      // Never drawn and in a container already on screen: a tab switch, not a
      // pane move, so the tab appears in place.
      Rect from = target.rect;
      if (lastDrawn != LastDrawn().end()) {
        from = lastDrawn->second;
      } else if (motion::Empty(onScreen) && !g_isNextEntryInPlace) {
        from = motion::EntryRect(target.rect, before);
      }
      newFlight.from.push_back({target.leaf, from});
      isMoving = isMoving || !motion::Same(from, target.rect);
    }
    g_isNextEntryInPlace = false;
    if (!isMoving) {
      if (hasFlight) {
        Flights().erase(flight);
      }
      Remember(contents, placed);
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
    Remember(contents, placed);
    return;
  }
  const double eased = motion::Ease(progress);
  for (size_t i = 0; i < placed.size(); i++) {
    placed[i].rect =
        motion::Lerp(flight->second.from[i].rect, placed[i].rect, eased);
  }
  Remember(contents, placed);
}

// Forget `mcv`'s flight, so the next layout starts a fresh one from where its
// panes are on screen now (after a caller has moved them there itself).
inline void Restart(void* mcv) {
  Flights().erase(mcv);
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
