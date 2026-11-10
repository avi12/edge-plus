// theme_wear.h — who put Edge on the theme it wears now: Edge+, or the user
// inside Edge ("Restore default" in edge://settings/appearance, a theme from a
// store). Pure decision, no Edge calls; core/wear_report.h tells the
// panel, which follows a user's choice of the default theme by clearing the
// slot of the current Windows mode.
//
// Edge goes back to its default through ThemeService::UseDefaultTheme, and not
// only when asked: while Edge+ switches from one theme to another, Edge turns
// the old theme extension off and may drop to the default before the new
// theme's pack is built. So a default Edge+ did not ask for is the user's only
// when no switch of Edge+'s is still settling.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "core/config.h"
#include "theme/theme_notice.h"

namespace edgeplus::theme_wear {

// @share WearCauseEdgePlus
inline constexpr uint32_t kCauseEdgePlus = 0;
// @share WearCauseUser
inline constexpr uint32_t kCauseUser = 1;

enum class Cause : uint32_t {
  kEdgePlus = kCauseEdgePlus,
  kUser = kCauseUser,
};

// A switch Edge+ started is over once Edge built the theme it asked for, or
// after this long (a theme Edge never builds must not mute the user for good).
inline constexpr uint64_t kSwitchSettleMs = 5000;

// Edge+'s last switch to an extension theme, until Edge built it.
class OwnSwitch {
 public:
  constexpr void Started(std::string_view themeId, uint64_t nowMs) noexcept {
    length_ = themeId.size() < sizeof(themeId_) ? themeId.size() : 0;
    for (size_t i = 0; i < length_; i++) {
      themeId_[i] = themeId[i];
    }
    startedMs_ = nowMs;
    isPending_ = length_ > 0;
  }

  constexpr void Built(std::string_view themeId) noexcept {
    const bool isTheAskedTheme = themeId == std::string_view(themeId_, length_);
    if (isTheAskedTheme) {
      isPending_ = false;
    }
  }

  [[nodiscard]] constexpr bool IsSettling(uint64_t nowMs) const noexcept {
    return isPending_ && nowMs - startedMs_ < kSwitchSettleMs;
  }

 private:
  char themeId_[config::kIdCapacity] = {};
  size_t length_ = 0;
  uint64_t startedMs_ = 0;
  bool isPending_ = false;
};

[[nodiscard]] constexpr Cause CauseOfDefault(
    bool isOwnCall, bool isOwnSwitchSettling) noexcept {
  const bool isEdgePlus = isOwnCall || isOwnSwitchSettling;
  return isEdgePlus ? Cause::kEdgePlus : Cause::kUser;
}

// A theme Edge+ was assigned is one it applied; any other the user chose.
[[nodiscard]] constexpr Cause CauseOfTheme(
    const theme_notice::Assigned& assigned, std::string_view themeId) noexcept {
  const bool isEdgePlus =
      assigned.isThemingActive && theme_notice::IsAssigned(assigned, themeId);
  return isEdgePlus ? Cause::kEdgePlus : Cause::kUser;
}

}  // namespace edgeplus::theme_wear
