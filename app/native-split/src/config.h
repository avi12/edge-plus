// config.h — the panel's live configuration of this DLL. One concern: the wire
// struct the panel writes to the config file and control.h applies, so its
// layout is written once here and mirrored field-for-field in
// app/panel/src-tauri/edge-native/src/protocol.rs. The panel owns which features are on and
// which theme to use for each OS mode; the DLL obeys live, with no restart.
#pragma once
#include <windows.h>
#include <cstdint>

namespace fixedge::config {

// Room for an extension id (32 chars) with slack; never spelled as a bare number.
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
};
// protocol.rs writes exactly WIRE_SIZE bytes and LoadFromFile wants all of
// them, so the struct must stay packed: five u32 fields, then the two ids.
inline constexpr size_t kWireFieldCount = 5;
inline constexpr size_t kWireIdCount = 2;
static_assert(sizeof(Wire) == kWireFieldCount * sizeof(uint32_t) +
                                  kWireIdCount * size_t{kIdCapacity},
              "Wire must stay packed as protocol.rs writes it");

// The same bytes the panel also writes beside its settings, so a DLL injected
// into an already-open Edge reads the current state at load. Live changes come
// the same way: the panel rewrites the file and signals kConfigEvent.
inline constexpr char kFileRelativePath[] = "\\FixEdge\\fixedge-config.bin";

inline bool LoadFromFile(Wire* out) {
  char localAppData[MAX_PATH] = {};
  const DWORD length =
      GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return false;
  }
  char path[MAX_PATH] = {};
  if (wsprintfA(path, "%s%s", localAppData, kFileRelativePath) <= 0) {
    return false;
  }
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  DWORD read = 0;
  const bool isWholeStruct =
      ReadFile(file, out, sizeof(Wire), &read, nullptr) && read == sizeof(Wire);
  CloseHandle(file);
  return isWholeStruct;
}

}  // namespace fixedge::config
