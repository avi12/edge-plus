// wear_report.h — the DLL telling the panel which theme Edge wears now and who
// put it there (theme/theme_wear.h), each time it changes. The panel shows "In
// use" from it and follows the user's own return to the default theme; it reads
// the file whenever its stamp changes. Mirrored in
// app/panel/src-tauri/src/dll/dll_wear.rs.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "core/atomic_file.h"
#include "core/config.h"
#include "theme/theme_wear.h"

namespace edgeplus::wear_report {

// Fixed-size and trivially copyable, like config::Wire.
struct Wire {
  uint32_t stamp;     // differs between two reports, so the panel sees each one
  uint32_t cause;     // theme_wear::Cause
  uint32_t idLength;  // 0 = Edge's default theme
  char id[config::kIdCapacity];
};
// @share WearFieldCount
inline constexpr size_t kFieldCount = 3;
static_assert(sizeof(Wire) ==
                  kFieldCount * sizeof(uint32_t) + size_t{config::kIdCapacity},
              "Wire must stay packed as dll_wear.rs reads it");

// @share WearFileRelativePath
inline constexpr char kFileRelativePath[] = "\\EdgePlus\\edgeplus-wear.bin";

inline constinit uint32_t g_lastStamp = 0;

inline void Report(std::string_view themeId, theme_wear::Cause cause) {
  Wire wire{};
  const bool canFit = themeId.size() <= sizeof(wire.id);
  if (!canFit) {
    return;
  }
  // The tick count tells reports of different Edge sessions apart; never
  // reusing a stamp keeps two in the same tick apart.
  const uint32_t ticks = GetTickCount();
  const uint32_t next = g_lastStamp + 1;
  g_lastStamp = ticks > next ? ticks : next;
  wire.stamp = g_lastStamp;
  wire.cause = static_cast<uint32_t>(cause);
  wire.idLength = static_cast<uint32_t>(themeId.size());
  themeId.copy(wire.id, themeId.size());
  atomic_file::WriteUnderLocalAppData(kFileRelativePath, &wire, sizeof(Wire));
}

}  // namespace edgeplus::wear_report
