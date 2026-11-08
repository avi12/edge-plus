// status.h — what the DLL tells the panel about the Edge it loaded into: whether
// it could place its hooks by the pinned table, found them by signature, or had
// to stand down, and which features that leaves. The panel pauses a feature only
// on this word, not on the version number alone (the reverse of config.h, which
// is the panel telling the DLL). Mirrored in app/panel-tauri/src-tauri/src/dll/
// dll_status.rs.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include "core/config.h"
#include "core/handles.h"

namespace edgeplus::status {

// How the DLL stands in this Edge.
inline constexpr uint32_t kModePinned = 0;  // the build offsets.h is pinned to
inline constexpr uint32_t kModeResolved =
    1;  // another build, hooks found by signature
inline constexpr uint32_t kModeRefused =
    2;  // another build, nothing safe to hook

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
inline constexpr size_t kFieldCount = 8;
static_assert(sizeof(Wire) == kFieldCount * sizeof(uint32_t),
              "Wire must stay packed as dll_status.rs reads it");

inline constexpr char kFileRelativePath[] = "\\EdgePlus\\edgeplus-status.bin";
inline constexpr char kTemporarySuffix[] = ".tmp";

// Written whole, then moved over the old file, so the panel never reads half of
// one. Best effort: a status that cannot be written leaves the panel on the
// version check alone.
inline void Write(const Wire& wire) {
  const std::string path = config::LocalAppDataPath(kFileRelativePath);
  if (path.empty()) {
    return;
  }
  const std::string temporaryPath = path + kTemporarySuffix;
  {
    const handles::UniqueHandle file{
        CreateFileA(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (!file) {
      return;
    }
    DWORD written = 0;
    const bool isWritten =
        WriteFile(file.Get(), &wire, sizeof(Wire), &written, nullptr) != FALSE;
    if (!isWritten) {
      return;
    }
  }
  MoveFileExA(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace edgeplus::status
