// remix_mode.h — Edge's "Create a remix" pages follow Edge's own light or dark
// and the Edge+ theme (docs/re/remix-theme.md). The switch only widens the set
// of pages theme_follow.h's hooks treat as followed (followed_pages.h); this
// header holds the switch. UI thread.
#pragma once
#include "core/config.h"
#include "core/log.h"
#include "theme/remix/remix_colors_store.h"

namespace edgeplus::remix_mode {

inline constinit bool g_isActive = false;

// True when the switch changed, so the caller asks every open profile to
// compute its pages again.
inline bool ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  g_isActive = wire.remixThemeActive != 0;
  const bool isTurnedOn = g_isActive && !wasActive;
  if (isTurnedOn) {
    remix_colors_store::Load();
  }
  const bool isChanged = wasActive != g_isActive;
  if (isChanged) {
    Log("[remix-theme] %s", g_isActive ? "on" : "off");
  }
  return isChanged;
}

}  // namespace edgeplus::remix_mode
