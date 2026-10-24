// live_preview.h — a hovered tab shows a live miniature, not a frozen snapshot.
// docs/re/live-preview.md.
//
// Edge draws the hover preview from a continuous capturer and the card already
// repaints every frame it receives; it looks static only because Edge stops the
// capturer after the first good frame. While a hover card is open we suppress
// that one stop.
#pragma once
#include "MinHook.h"
#include "config.h"
#include "edge.h"
#include "log.h"

namespace edgeplus::live_preview {

inline constinit bool g_isActive = false;
// A bool, not a counter: Observe fires again when a hover retargets to another
// tab, and only one hover card is ever open.
inline constinit bool g_isCardOpen = false;

// ThumbnailTabHelper::TabStateTracker::StopCapture: not per-frame.
using StopCapture_t = void(__fastcall*)(void*);
inline constinit StopCapture_t o_stop_capture = nullptr;
inline void __fastcall h_stop_capture(void* tracker) {
  const bool isHoldingForPreview = g_isActive && g_isCardOpen;
  if (isHoldingForPreview) {
    return;
  }
  o_stop_capture(tracker);
}

// TabHoverCardThumbnailObserver::Observe: a preview is on screen.
using Observe_t = void(__fastcall*)(void*, void*);
inline constinit Observe_t o_observe = nullptr;
inline void __fastcall h_observe(void* observer, void* thumbnailImage) {
  g_isCardOpen = true;
  o_observe(observer, thumbnailImage);
}

// TabHoverCardController::OnCardClosing: Edge's next StopCapture then winds the
// capturer down normally.
using OnCardClosing_t = void(__fastcall*)(void*);
inline constinit OnCardClosing_t o_on_card_closing = nullptr;
inline void __fastcall h_on_card_closing(void* controller) {
  g_isCardOpen = false;
  o_on_card_closing(controller);
}

inline bool EnableSite(uintptr_t site) {
  const MH_STATUS status = MH_EnableHook(edge::At(site));
  return status == MH_OK || status == MH_ERROR_ENABLED;
}

inline bool EnableAll() {
  return EnableSite(
             edge155::rva::ThumbnailTabHelper_TabStateTracker_StopCapture) &&
         EnableSite(edge155::rva::TabHoverCardThumbnailObserver_Observe) &&
         EnableSite(edge155::rva::TabHoverCardController_OnCardClosing);
}

inline bool Install() {
  const bool isHooked =
      MH_CreateHook(
          edge::At(
              edge155::rva::ThumbnailTabHelper_TabStateTracker_StopCapture),
          reinterpret_cast<void*>(h_stop_capture),
          reinterpret_cast<void**>(&o_stop_capture)) == MH_OK &&
      MH_CreateHook(
          edge::At(edge155::rva::TabHoverCardThumbnailObserver_Observe),
          reinterpret_cast<void*>(h_observe),
          reinterpret_cast<void**>(&o_observe)) == MH_OK &&
      MH_CreateHook(
          edge::At(edge155::rva::TabHoverCardController_OnCardClosing),
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

}  // namespace edgeplus::live_preview
