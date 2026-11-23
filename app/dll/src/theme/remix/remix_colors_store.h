// remix_colors_store.h — the light and dark colour pair of Create a remix's
// Appearance remixes (remix_colors.h), kept in a file of the DLL's own in the
// Edge+ data folder and held here while the remix switch is on. The pair is
// global, as the theme pair is: one colour per Edge mode for every Appearance
// remix, applied at script build time (remix_darkmode.h). UI thread.
#pragma once
#include <windows.h>
#include <string>
#include "core/config.h"
#include "core/handles.h"
#include "core/log.h"
#include "theme/remix/remix_colors.h"

namespace edgeplus::remix_colors_store {

inline constexpr char kRelativePath[] = "\\EdgePlus\\remix-colors.txt";
// A pair's text is a few dozen bytes.
inline constexpr DWORD kMaxFileBytes = 256;

inline constinit remix_colors::Pair g_pair{};

inline void Load() {
  g_pair = remix_colors::Pair{};
  const std::string path = config::LocalAppDataPath(kRelativePath);
  const handles::UniqueHandle file{
      path.empty()
          ? INVALID_HANDLE_VALUE
          : CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!file) {
    return;
  }
  std::string text(kMaxFileBytes, '\0');
  DWORD read = 0;
  const bool isRead =
      ReadFile(file.Get(), text.data(), kMaxFileBytes, &read, nullptr) != 0;
  if (!isRead) {
    return;
  }
  text.resize(read);
  remix_colors::Pair loaded;
  if (remix_colors::ParsePair(text, &loaded)) {
    g_pair = loaded;
  }
}

inline void Save(const remix_colors::Pair& pair) {
  g_pair = pair;
  const std::string path = config::LocalAppDataPath(kRelativePath);
  const handles::UniqueHandle file{
      path.empty()
          ? INVALID_HANDLE_VALUE
          : CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!file) {
    Log("[remix-theme] could not save the colour pair (error %lu, %s)",
        GetLastError(), path.c_str());
    return;
  }
  const std::string text = remix_colors::FormatPair(pair);
  DWORD written = 0;
  WriteFile(file.Get(), text.data(), static_cast<DWORD>(text.size()), &written,
            nullptr);
  Log("[remix-theme] colour pair saved: %s", text.c_str());
}

}  // namespace edgeplus::remix_colors_store
