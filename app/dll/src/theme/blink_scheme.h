// blink_scheme.h — how Blink stores a page's preferred color scheme. Pure.
#pragma once
#include <cstdint>

namespace edgeplus::blink_scheme {

// Blink's PreferredColorScheme; the scrollbar scheme is the other way round.
enum class BlinkScheme : int32_t {
  kDark = 0,
  kLight = 1,
};

[[nodiscard]] constexpr int32_t PageSchemeOf(bool isDark) noexcept {
  return static_cast<int32_t>(isDark ? BlinkScheme::kDark
                                     : BlinkScheme::kLight);
}

[[nodiscard]] constexpr int32_t ScrollbarSchemeOf(bool isDark) noexcept {
  return PageSchemeOf(!isDark);
}

[[nodiscard]] constexpr bool IsDarkPageScheme(int32_t scheme) noexcept {
  return scheme == PageSchemeOf(true);
}

}  // namespace edgeplus::blink_scheme
