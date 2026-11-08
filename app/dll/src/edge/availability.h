// availability.h — which of the panel's features can run in this Edge build. On the
// pinned build all of them; on another build resolve.h clears the bits of every
// feature that has an address it could not place, so that feature stays off while
// the rest keep working.
#pragma once
#include <cstdint>
#include "core/config.h"
#include "edge/signature_table.h"

namespace edgeplus::availability {

namespace signatures = ::edgeplus::edge155::signatures;

inline constinit uint16_t g_features = signatures::kEveryFeature;

[[nodiscard]] inline bool IsAvailable(signatures::Feature feature) {
  return (g_features & signatures::BitOf(feature)) != 0;
}

// The panel's choice, with every feature this build cannot run switched off.
inline void Mask(config::Wire* wire) {
  struct Switch {
    uint32_t config::Wire::* field;
    signatures::Feature feature;
  };
  static constexpr Switch kSwitches[] = {
      {&config::Wire::splitActive, signatures::Feature::kSplit},
      {&config::Wire::themingActive, signatures::Feature::kTheming},
      {&config::Wire::protocolHandlersActive,
       signatures::Feature::kProtocolHandlers},
      {&config::Wire::enhancedFindActive, signatures::Feature::kEnhancedFind},
      {&config::Wire::webUiFollowsThemeActive,
       signatures::Feature::kWebUiFollowsTheme},
      {&config::Wire::livePreviewActive, signatures::Feature::kLivePreview},
      {&config::Wire::cyberpunkSkinActive, signatures::Feature::kCyberpunkSkin},
      {&config::Wire::readingThemeActive, signatures::Feature::kReadingTheme},
  };
  for (const Switch& item : kSwitches) {
    if (!IsAvailable(item.feature)) {
      wire->*(item.field) = 0;
    }
  }
}

}  // namespace edgeplus::availability
