// edge_url.h — Chrome's internal pages, as Edge's. One concern, pure (no Edge,
// no Win32; unit-tested in test/edge_url_test.cpp): Gemini's panel asks to open
// Chrome's own pages (its settings, its password manager), which Edge does not
// have under those names. This maps each to the closest page Edge does have.
#pragma once
#include <string>
#include <string_view>

namespace fixedge::edge_url {

inline constexpr std::string_view kChromeScheme = "chrome://";
inline constexpr std::string_view kEdgeScheme = "edge://";

// Chrome pages with a differently-named counterpart in Edge, by URL prefix
// (first match wins). Gemini's settings are Edge's AI settings.
struct Mapping {
  std::string_view chromePrefix;
  std::string_view edgeUrl;
};
inline constexpr Mapping kMappings[] = {
    {"chrome://settings/ai", "edge://settings/ai"},
    {"chrome://settings/gemini", "edge://settings/ai"},
    {"chrome://password-manager", "edge://wallet/passwords"},
    {"chrome://settings/passwords", "edge://wallet/passwords"},
    {"chrome://settings/addresses", "edge://settings/profiles"},
    {"chrome://settings/payments", "edge://wallet/payments"},
};

inline bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

// The Edge page for `url`: a mapped page, else the same path under edge://
// (most settings pages share their names), else `url` itself.
inline std::string EdgeUrlFor(const std::string& url) {
  for (const Mapping& mapping : kMappings) {
    if (StartsWith(url, mapping.chromePrefix)) {
      return std::string(mapping.edgeUrl);
    }
  }
  if (StartsWith(url, kChromeScheme)) {
    return std::string(kEdgeScheme) + url.substr(kChromeScheme.size());
  }
  return url;
}

// Whether a page may ask for `url` to be opened: the web and Edge's own pages,
// never a script or a local file.
inline bool IsOpenable(const std::string& url) {
  return StartsWith(url, "https://") || StartsWith(url, "http://") ||
         StartsWith(url, kEdgeScheme);
}

}  // namespace fixedge::edge_url
