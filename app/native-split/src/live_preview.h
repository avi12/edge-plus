// live_preview.h — a hovered tab shows a LIVE miniature, not a frozen snapshot.
// One concern: keep Edge's own tab-thumbnail video capturer running while a hover
// card is open, so the frames it already produces keep reaching the card. See
// docs/re/live-preview.md.
//
// Edge draws the hover preview from a continuous capturer
// (BackgroundThumbnailVideoCapturer) and the card already repaints every frame it
// receives; it looks static only because Edge stops the capturer after the first
// good frame (ThumbnailTabHelper::TabStateTracker::StopCapture). While the feature
// is on and a hover card is open, we suppress that one stop and the existing
// pipeline delivers live video — no new capture, no mirroring. A card's thumbnail
// subscription beginning (TabHoverCardThumbnailObserver::Observe) marks a preview
// on screen; the card closing (TabHoverCardController::OnCardClosing) ends it.
// Everything else, and the feature off, keeps Edge's behaviour exactly.
#pragma once
#include "MinHook.h"
#include "config.h"
#include "edge.h"
#include "log.h"

namespace fixedge::live_preview {

using namespace fixedge::edge155;

// The feature switch, and whether a hover card is on screen right now, so the
// capturer is held open only while one is. Observe sets it, OnCardClosing clears
// it — a bool, not a counter, because Observe fires again when a hover retargets
// to another tab and only ever one hover card is open at a time.
inline bool g_isActive = false;
inline bool g_isCardOpen = false;

// ThumbnailTabHelper::TabStateTracker::StopCapture: Edge's "end this capture
// session" call. While a hover card is open we return without stopping, so the
// capturer keeps delivering frames and the card goes live; otherwise Edge stops
// as usual. Not a per-frame call, so this runs rarely.
using StopCapture_t = void(__fastcall*)(void*);
inline StopCapture_t o_stop_capture = nullptr;
inline void __fastcall h_stop_capture(void* tracker) {
  const bool isHoldingForPreview = g_isActive && g_isCardOpen;
  if (isHoldingForPreview) {
    return;
  }
  o_stop_capture(tracker);
}

// TabHoverCardThumbnailObserver::Observe: a hover card's live thumbnail
// subscription begins, i.e. a preview is on screen and frames are wanted.
using Observe_t = void(__fastcall*)(void*, void*);
inline Observe_t o_observe = nullptr;
inline void __fastcall h_observe(void* observer, void* thumbnailImage) {
  g_isCardOpen = true;
  o_observe(observer, thumbnailImage);
}

// TabHoverCardController::OnCardClosing: the hover card is gone, so stop holding
// the capturer open; Edge's next StopCapture then winds it down normally.
using OnCardClosing_t = void(__fastcall*)(void*);
inline OnCardClosing_t o_on_card_closing = nullptr;
inline void __fastcall h_on_card_closing(void* controller) {
  g_isCardOpen = false;
  o_on_card_closing(controller);
}

// --- install / apply -----------------------------------------------------

inline bool EnableSite(uintptr_t site) {
  const MH_STATUS status = MH_EnableHook(edge::At(site));
  return status == MH_OK || status == MH_ERROR_ENABLED;
}

inline bool EnableAll() {
  return EnableSite(rva::ThumbnailTabHelper_TabStateTracker_StopCapture) &&
         EnableSite(rva::TabHoverCardThumbnailObserver_Observe) &&
         EnableSite(rva::TabHoverCardController_OnCardClosing);
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(
          edge::At(rva::ThumbnailTabHelper_TabStateTracker_StopCapture),
          reinterpret_cast<void*>(h_stop_capture),
          reinterpret_cast<void**>(&o_stop_capture)) == MH_OK &&
      MH_CreateHook(edge::At(rva::TabHoverCardThumbnailObserver_Observe),
                    reinterpret_cast<void*>(h_observe),
                    reinterpret_cast<void**>(&o_observe)) == MH_OK &&
      MH_CreateHook(edge::At(rva::TabHoverCardController_OnCardClosing),
                    reinterpret_cast<void*>(h_on_card_closing),
                    reinterpret_cast<void**>(&o_on_card_closing)) == MH_OK;
  if (!isHooked) {
    Log("[err] live-preview hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] live-preview hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

// Take the panel's switch. Turning it off also forgets any open-card state, so a
// later StopCapture is never suppressed by a stale flag.
inline void ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  g_isActive = wire.livePreviewActive != 0;
  if (wasActive != g_isActive) {
    Log("[live-preview] %s", g_isActive ? "on" : "off");
  }
  if (!g_isActive) {
    g_isCardOpen = false;
  }
}

}  // namespace fixedge::live_preview
