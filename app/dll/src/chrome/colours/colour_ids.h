// colour_ids.h — Edge's colour ids by name. msedge.dll carries the name of every
// colour id (Chromium's `kColor...` enumerators and Edge's own) in tables of
// 16-byte entries {const char* name, uint64 id + 1}, one run per id range. This
// finds those entries in the loaded image, so the skin's colour roles
// (colour_roles.h) are placed by name and need no id from the build they were
// written for: ids move between Edge builds, the names do not. The scan reads
// the image's read-only data once, when the skin first turns on (a few hundred
// milliseconds), on the UI thread.
#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>
#include "chrome/colours/colour_roles.h"
#include "core/log.h"
#include "edge/offsets.h"
#include "theme/cyberpunk_palette.h"

namespace edgeplus::colour_ids {

using cyberpunk_palette::Role;

// Above every id this Edge defines (the last is near 0xd50).
inline constexpr int kMaxId = 0x1000;

inline constexpr std::string_view kNamePrefix = "kColor";
inline constexpr size_t kLongestName = 96;
inline constexpr uint64_t kEntryAlignment = sizeof(uint64_t);

// A role per colour id; kNone (0) for every id the skin leaves alone.
inline constinit std::array<Role, kMaxId> g_roleById{};
inline constinit bool g_isResolved = false;

[[nodiscard]] inline Role RoleOf(int colorId) noexcept {
  const bool isInRange = colorId >= 0 && colorId < kMaxId;
  if (!g_isResolved || !isInRange) {
    return Role::kNone;
  }
  return g_roleById[static_cast<size_t>(colorId)];
}

struct NamedId {
  std::string_view name;
  int id;
};

// Every colour name Edge's tables hold, by id, kept for the tools that print
// names (colour_dump.h). Points into msedge.dll's own strings.
inline constinit std::vector<NamedId> g_names;

[[nodiscard]] inline std::string_view NameOf(int colorId) {
  for (const NamedId& named : g_names) {
    if (named.id == colorId) {
      return named.name;
    }
  }
  return "?";
}

// A run of the image the loader mapped readable and not executable: strings and
// the name tables sit there.
struct Span {
  uintptr_t begin;
  uintptr_t end;
};

[[nodiscard]] inline std::vector<Span> ReadOnlyData(uintptr_t base) {
  std::vector<Span> spans;
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* ntHeaders =
      reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);
  for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
    const DWORD flags = section[i].Characteristics;
    const bool isReadOnlyData = (flags & IMAGE_SCN_MEM_READ) != 0 &&
                                (flags & IMAGE_SCN_MEM_EXECUTE) == 0 &&
                                (flags & IMAGE_SCN_MEM_DISCARDABLE) == 0;
    if (!isReadOnlyData) {
      continue;
    }
    // Only pages that are committed and readable now: the loader may leave a
    // page of a section without access, and a read there would crash Edge.
    const uintptr_t begin = base + section[i].VirtualAddress;
    const uintptr_t end = begin + section[i].Misc.VirtualSize;
    uintptr_t cursor = begin;
    while (cursor < end) {
      MEMORY_BASIC_INFORMATION region{};
      const bool isQueried = VirtualQuery(reinterpret_cast<void*>(cursor),
                                          &region, sizeof(region)) != 0;
      if (!isQueried) {
        break;
      }
      const uintptr_t regionEnd = std::min(
          reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize,
          end);
      const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY;
      const bool isReadable = region.State == MEM_COMMIT &&
                              (region.Protect & readable) != 0 &&
                              (region.Protect & PAGE_GUARD) == 0;
      if (isReadable) {
        spans.push_back({cursor, regionEnd});
      }
      cursor = regionEnd;
    }
  }
  return spans;
}

[[nodiscard]] inline bool IsInside(const std::vector<Span>& spans,
                                   uintptr_t address, size_t length) {
  for (const Span& span : spans) {
    // Subtracting, so an address near the top of the range cannot wrap round.
    const bool isInSpan = address >= span.begin && address < span.end &&
                          length <= span.end - address;
    if (isInSpan) {
      return true;
    }
  }
  return false;
}

// The `kColor...` string a table entry names, or empty when the pointer is
// not one.
[[nodiscard]] inline std::string_view NameAt(const std::vector<Span>& spans,
                                             uint64_t pointer) {
  const bool isNameStart =
      IsInside(spans, pointer, kNamePrefix.size()) &&
      std::memcmp(reinterpret_cast<const char*>(pointer), kNamePrefix.data(),
                  kNamePrefix.size()) == 0;
  if (!isNameStart) {
    return {};
  }
  const auto* text = reinterpret_cast<const char*>(pointer);
  size_t length = kNamePrefix.size();
  while (length < kLongestName && IsInside(spans, pointer + length, 1) &&
         text[length] != '\0') {
    length++;
  }
  const bool isTerminated = length < kLongestName && text[length] == '\0';
  return isTerminated ? std::string_view(text, length) : std::string_view();
}

[[nodiscard]] inline std::vector<NamedId> ReadNameTables(uintptr_t base) {
  std::vector<NamedId> found;
  const std::vector<Span> spans = ReadOnlyData(base);
  for (const Span& span : spans) {
    for (uintptr_t address = span.begin;
         address + 2 * sizeof(uint64_t) <= span.end;
         address += kEntryAlignment) {
      const auto* entry = reinterpret_cast<const uint64_t*>(address);
      const uint64_t idPlusOne = entry[1];
      const bool isIdShaped = idPlusOne >= 1 && idPlusOne <= kMaxId;
      if (!isIdShaped) {
        continue;
      }
      const std::string_view name = NameAt(spans, entry[0]);
      if (!name.empty()) {
        found.push_back({name, static_cast<int>(idPlusOne - 1)});
      }
    }
  }
  return found;
}

// Fills g_roleById from colour_roles.h's names; logs those this build has no
// colour of.
inline void Resolve() {
  if (g_isResolved) {
    return;
  }
  const auto base =
      reinterpret_cast<uintptr_t>(GetModuleHandleW(edge155::kModule));
  if (base == 0) {
    return;
  }
  Log("[colours] reading Edge's colour name tables");
  std::vector<NamedId> table = ReadNameTables(base);
  Log("[colours] read %zu names", table.size());
  std::sort(table.begin(), table.end(),
            [](const NamedId& first, const NamedId& second) {
              return first.name < second.name;
            });
  int missing = 0;
  int placed = 0;
  for (const colour_roles::Entry& entry : colour_roles::kEntries) {
    const auto hit =
        std::lower_bound(table.begin(), table.end(), entry.name,
                         [](const NamedId& named, std::string_view name) {
                           return named.name < name;
                         });
    const bool isFound = hit != table.end() && hit->name == entry.name;
    if (isFound) {
      g_roleById[static_cast<size_t>(hit->id)] = entry.role;
      placed++;
    } else {
      missing++;
      Log("[colours] this Edge has no %.*s",
          static_cast<int>(entry.name.size()), entry.name.data());
    }
  }
  g_names = table;
  g_isResolved = true;
  Log("[colours] %zu names in Edge's tables; %d skin colours placed, %d "
      "missing",
      table.size(), placed, missing);
}

}  // namespace edgeplus::colour_ids
