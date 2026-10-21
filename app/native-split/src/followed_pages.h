// followed_pages.h — which pages follow the theme's light or dark. One concern:
// the pure test of a page's URL against the two Edge pages (settings and
// extensions) that theme_follow.h drives; every other page, ordinary or WebUI,
// keeps what Edge decides. No Edge, no Win32.
#pragma once
#include <string_view>

namespace fixedge::followed_pages {

// The pages, by the address Edge shows (and keeps as the committed URL).
inline constexpr std::string_view kSettings = "edge://settings";
inline constexpr std::string_view kExtensions = "edge://extensions";

// A page's address continues past the host only with a path, query or
// fragment, so "edge://settingsfoo" is another host.
[[nodiscard]] inline bool IsHostEnd(std::string_view rest) {
  if (rest.empty()) {
    return true;
  }
  const char next = rest.front();
  return next == '/' || next == '?' || next == '#';
}

[[nodiscard]] inline bool IsPageOf(std::string_view url,
                                   std::string_view page) {
  return url.starts_with(page) && IsHostEnd(url.substr(page.size()));
}

// Whether the page at `url` follows the theme.
[[nodiscard]] inline bool Follows(std::string_view url) {
  return IsPageOf(url, kSettings) || IsPageOf(url, kExtensions);
}

}  // namespace fixedge::followed_pages
