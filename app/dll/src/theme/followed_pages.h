// followed_pages.h — which pages theme_follow.h makes follow the theme's light
// or dark; every other page keeps what Edge decides. Two switches own two
// groups: the web UI follows (settings, extensions) and Create a remix's pages.
// Pure.
#pragma once
#include <string_view>

namespace edgeplus::followed_pages {

// As Edge shows them and keeps them as the committed URL.
inline constexpr std::string_view kSettings = "edge://settings";
inline constexpr std::string_view kExtensions = "edge://extensions";
// "Create a remix": the bubble's page and the manager side panel.
inline constexpr std::string_view kBlueprint = "edge://blueprint";
inline constexpr std::string_view kRemix = "edge://remix";

// Which switches are on.
struct Switches {
  bool isWebUiFollowing = false;
  bool isRemixFollowing = false;
};

[[nodiscard]] constexpr bool IsAnyOn(const Switches& switches) noexcept {
  return switches.isWebUiFollowing || switches.isRemixFollowing;
}

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

[[nodiscard]] inline bool Follows(std::string_view url,
                                  const Switches& switches) {
  const bool isWebUiPage =
      IsPageOf(url, kSettings) || IsPageOf(url, kExtensions);
  const bool isRemixPage = IsPageOf(url, kBlueprint) || IsPageOf(url, kRemix);
  return (switches.isWebUiFollowing && isWebUiPage) ||
         (switches.isRemixFollowing && isRemixPage);
}

}  // namespace edgeplus::followed_pages
