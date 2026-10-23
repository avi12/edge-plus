// config.h — the panel's live configuration of this DLL. One concern: the wire
// struct the panel writes to the config file and control.h applies, so its
// layout is written once here and mirrored field-for-field in
// app/panel-winui/Panel/Dll/ConfigWire.cs (the @share markers hand the sizes
// and the file to the panel; tools/Export-SharedConstants.ps1). The panel owns
// which features are on and which theme to use for each OS mode; the DLL obeys
// live, with no restart.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include "handles.h"

namespace fixedge::config {

// Room for an extension id (32 chars) with slack; never spelled as a bare number.
// @share WireIdCapacity
inline constexpr uint32_t kIdCapacity = 64;

// The whole desired state, sent whenever the user changes a switch or a theme
// assignment. Fixed-size and trivially-copyable so it crosses the process
// boundary as plain bytes. An empty id length means "use the default theme".
struct Wire {
  uint32_t splitActive;             // 1 = the split feature is on
  uint32_t themingActive;           // 1 = the theming feature is on
  uint32_t protocolHandlersActive;  // 1 = ask to handle links, like Chrome
  uint32_t lightLength;  // bytes of lightId in use (0 = default theme)
  uint32_t darkLength;   // bytes of darkId in use (0 = default theme)
  char lightId[kIdCapacity];
  char darkId[kIdCapacity];
  // Appended after the ids, so a DLL built before each still reads the fields
  // above it and a file written before it still loads (as off).
  uint32_t reserved;  // once the chosen assistant; the panel writes 0, unread
  uint32_t enhancedFindActive;       // 1 = Ctrl+F opens the enhanced find bar
  uint32_t webUiFollowsThemeActive;  // 1 = settings and extensions pages follow
                                     // the theme's light or dark
  uint32_t livePreviewActive;        // 1 = a hovered tab shows a live preview
  uint32_t tabShapeActive;      // 1 = tabs get cut corners (with theming on)
  uint32_t readingThemeActive;  // 1 = reading mode follows Windows light/dark
                                // (the two themes are chosen in reading mode
                                // itself and kept by reading_mode.h)
};
// ConfigWire.cs writes exactly WIRE_SIZE bytes and LoadFromFile wants all of
// them, so the struct must stay packed: five u32 fields, the two ids, then the
// reserved field, the find, WebUI, live-preview, tab-shape and reading-theme
// switches (u32s). A panel that still writes the two reading themes after the
// switch (180 bytes) is read for its first sizeof(Wire) bytes.
// @share WireFieldCount
inline constexpr size_t kWireFieldCount = 11;
// @share WireIdCount
inline constexpr size_t kWireIdCount = 2;
static_assert(sizeof(Wire) == kWireFieldCount * sizeof(uint32_t) +
                                  kWireIdCount * size_t{kIdCapacity},
              "Wire must stay packed as ConfigWire.cs writes it");

// The sizes of the files a panel wrote before `reserved`, before
// `enhancedFindActive`, before `webUiFollowsThemeActive`, before
// `livePreviewActive`, before `tabShapeActive` and before `readingThemeActive`
// existed.
inline constexpr DWORD kLegacyWireSize = offsetof(Wire, reserved);
inline constexpr DWORD kBeforeFindWireSize = offsetof(Wire, enhancedFindActive);
inline constexpr DWORD kBeforeWebUiWireSize =
    offsetof(Wire, webUiFollowsThemeActive);
inline constexpr DWORD kBeforeLivePreviewWireSize =
    offsetof(Wire, livePreviewActive);
inline constexpr DWORD kBeforeTabShapeWireSize = offsetof(Wire, tabShapeActive);
inline constexpr DWORD kBeforeReadingThemeWireSize =
    offsetof(Wire, readingThemeActive);

// The same bytes the panel also writes beside its settings, so a DLL injected
// into an already-open Edge reads the current state at load. Live changes come
// the same way: the panel rewrites the file and signals kConfigEvent.
// @share ConfigFileRelativePath
inline constexpr char kFileRelativePath[] = "\\FixEdge\\fixedge-config.bin";

// `relativePath` (such as kFileRelativePath) under %LOCALAPPDATA%, or empty
// when that is unset.
[[nodiscard]] inline std::string LocalAppDataPath(const char* relativePath) {
  char localAppData[MAX_PATH] = {};
  const DWORD length =
      GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
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

}  // namespace fixedge::config
