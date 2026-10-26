// followed_pages.h — which pages theme_follow.h makes follow the theme's light
// or dark; every other page keeps what Edge decides. Pure.
#pragma once
#include <string_view>

namespace edgeplus::followed_pages {

// As Edge shows them and keeps them as the committed URL.
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

[[nodiscard]] inline bool Follows(std::string_view url) {
  return IsPageOf(url, kSettings) || IsPageOf(url, kExtensions);
}

}  // namespace edgeplus::followed_pages
