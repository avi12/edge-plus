// theme_hold.h — a theme Edge+ installs but does not wear. Edge makes a theme it
// has just installed the active one; a theme the user added without Shift is to
// stay installed and off, so when Edge's build of it lands for the held id, the
// swap is not let through (hooks.h). Pure decision, no Edge and no Win32.
#pragma once
#include <string_view>

namespace edgeplus::theme_hold {

// True when Edge's making `themeId` active must be stopped: it is the held
// theme, and Edge+ did not ask for it itself (`isOwnSwitch`), which is how a
// theme the user later picks for a mode still gets worn.
[[nodiscard]] constexpr bool ShouldKeepOff(std::string_view heldId,
                                           std::string_view themeId,
                                           bool isOwnSwitch) noexcept {
  const bool isHeld = !heldId.empty() && themeId == heldId;
  return isHeld && !isOwnSwitch;
}

}  // namespace edgeplus::theme_hold
