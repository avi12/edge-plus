// status.h — what the DLL tells the panel about the Edge it loaded into: whether
// it could place its hooks by the pinned table, found them by signature, or had
// to stand down, and which features that leaves. The panel pauses a feature only
// on this word, not on the version number alone (the reverse of config.h, which
// is the panel telling the DLL). Mirrored in app/panel/src-tauri/src/dll/
// dll_status.rs.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include "core/atomic_file.h"

namespace edgeplus::status {

// How the DLL stands in this Edge: on the build offsets.h is pinned to; on
// another, with the features whose code it found unchanged; or refused,
// nothing safe to hook.
// @share StatusModePinned
inline constexpr uint32_t kModePinned = 0;
// @share StatusModeResolved
inline constexpr uint32_t kModeResolved = 1;
// @share StatusModeRefused
inline constexpr uint32_t kModeRefused = 2;

enum class Mode : uint32_t {
  kPinned = kModePinned,
  kResolved = kModeResolved,
  kRefused = kModeRefused,
};

// Fixed-size and trivially copyable, like config::Wire.
struct Wire {
  uint32_t mode;
  uint32_t major;  // the msedge.dll the DLL loaded into
  uint32_t minor;
  uint32_t build;
  uint32_t revision;
  uint32_t availableFeatures;  // the Feature bits that can run here
  uint32_t resolvedEntries;    // offsets.h entries placed by signature
  uint32_t totalEntries;
};
// @share StatusFieldCount
inline constexpr size_t kFieldCount = 8;
static_assert(sizeof(Wire) == kFieldCount * sizeof(uint32_t),
              "Wire must stay packed as dll_status.rs reads it");

// @share StatusFileRelativePath
inline constexpr char kFileRelativePath[] = "\\EdgePlus\\edgeplus-status.bin";

// Best effort: a status that cannot be written leaves the panel on the version
// check alone.
inline void Write(const Wire& wire) {
  atomic_file::WriteUnderLocalAppData(kFileRelativePath, &wire, sizeof(Wire));
}

}  // namespace edgeplus::status
