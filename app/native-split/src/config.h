// config.h — the panel's live configuration of this DLL: the wire struct the
// panel writes to the config file, mirrored field-for-field in
// app/panel-tauri/src-tauri/src/dll/config_wire.rs.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include "handles.h"

namespace edgeplus::config {

// An extension id (32 chars) with slack.
// @share WireIdCapacity
inline constexpr uint32_t kIdCapacity = 64;

// Fixed-size and trivially-copyable so it crosses the process boundary as plain
// bytes. Each switch is 1 for on.
struct Wire {
  uint32_t splitActive;
  uint32_t themingActive;
  uint32_t protocolHandlersActive;  // ask to handle links, like Chrome
  uint32_t lightLength;             // 0 = default theme
  uint32_t darkLength;              // 0 = default theme
  char lightId[kIdCapacity];
  char darkId[kIdCapacity];
  // Appended after the ids, so a DLL built before each still reads the fields
  // above it and a file written before it still loads (as off).
  uint32_t reserved;  // once the chosen assistant; the panel writes 0, unread
  uint32_t enhancedFindActive;
  uint32_t webUiFollowsThemeActive;
  uint32_t livePreviewActive;
  uint32_t tabShapeActive;      // with theming on
  uint32_t readingThemeActive;  // its two themes are kept by reading_mode.h
};
// ConfigWire.cs writes exactly this many bytes, so the struct must stay packed.
// A panel that still writes the two reading themes after the switch (180 bytes)
// is read for its first sizeof(Wire) bytes.
// @share WireFieldCount
inline constexpr size_t kWireFieldCount = 11;
// @share WireIdCount
inline constexpr size_t kWireIdCount = 2;
static_assert(sizeof(Wire) == kWireFieldCount * sizeof(uint32_t) +
                                  kWireIdCount * size_t{kIdCapacity},
              "Wire must stay packed as ConfigWire.cs writes it");

// The sizes of files written before each appended field existed.
inline constexpr DWORD kLegacyWireSize = offsetof(Wire, reserved);
inline constexpr DWORD kBeforeFindWireSize = offsetof(Wire, enhancedFindActive);
inline constexpr DWORD kBeforeWebUiWireSize =
    offsetof(Wire, webUiFollowsThemeActive);
inline constexpr DWORD kBeforeLivePreviewWireSize =
    offsetof(Wire, livePreviewActive);
inline constexpr DWORD kBeforeTabShapeWireSize = offsetof(Wire, tabShapeActive);
inline constexpr DWORD kBeforeReadingThemeWireSize =
    offsetof(Wire, readingThemeActive);

// A DLL injected into an already-open Edge reads the current state here at
// load; on a change the panel rewrites it and signals kConfigEvent.
// @share ConfigFileRelativePath
inline constexpr char kFileRelativePath[] = "\\EdgePlus\\edgeplus-config.bin";

inline constexpr char kLocalAppDataVariable[] = "LOCALAPPDATA";

// Empty when %LOCALAPPDATA% is unset.
[[nodiscard]] inline std::string LocalAppDataPath(const char* relativePath) {
  char localAppData[MAX_PATH] = {};
  const DWORD length =
      GetEnvironmentVariableA(kLocalAppDataVariable, localAppData, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return std::string();
  }
  return std::string(localAppData, length) + relativePath;
}

[[nodiscard]] inline bool LoadFromFile(Wire* out) {
  const std::string path = LocalAppDataPath(kFileRelativePath);
  if (path.empty()) {
    return false;
  }
  const handles::UniqueHandle file{
      CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!file) {
    return false;
  }
  *out = Wire{};
  DWORD read = 0;
  const bool isRead = ReadFile(file.Get(), out, sizeof(Wire), &read, nullptr);
  const bool isKnownSize =
      isRead &&
      (read == sizeof(Wire) || read == kBeforeReadingThemeWireSize ||
       read == kBeforeTabShapeWireSize || read == kBeforeLivePreviewWireSize ||
       read == kBeforeWebUiWireSize || read == kBeforeFindWireSize ||
       read == kLegacyWireSize);
  return isKnownSize;
}

}  // namespace edgeplus::config
