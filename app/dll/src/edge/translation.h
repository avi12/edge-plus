// translation.h — where offsets.h's addresses really are in the running msedge.dll.
//
// On the build offsets.h is pinned to nothing changes: an address is its own
// translation. On another build resolve.h finds each entry by signature and
// fills this table, keyed by the pinned value, so every `edge::At(rva::X)` of the
// DLL lands on the right code without knowing which case it is in. An address the
// table does not hold (never found, or its feature stood down) translates to
// nothing, and edge::At hands back a null pointer rather than a stale one.
#pragma once
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace edgeplus::edge::translation {

inline constexpr uintptr_t kUnresolved = ~uintptr_t{0};

// pinned RVA -> RVA in this build.
using Placement = std::pair<uintptr_t, uintptr_t>;

inline constinit bool g_isActive = false;

// Sorted by the pinned RVA; filled once by resolve.h before any hook is
// installed, then only read.
inline constinit std::vector<Placement> g_table;

inline void Fill(std::vector<Placement> placements) {
  std::sort(placements.begin(), placements.end());
  g_table = std::move(placements);
}

// The RVA in the running msedge.dll for an RVA of offsets.h.
[[nodiscard]] inline uintptr_t Translate(uintptr_t pinnedRva) noexcept {
  if (!g_isActive) {
    return pinnedRva;
  }
  const auto found =
      std::lower_bound(g_table.begin(), g_table.end(), pinnedRva,
                       [](const Placement& placement, uintptr_t key) noexcept {
                         return placement.first < key;
                       });
  const bool isFound = found != g_table.end() && found->first == pinnedRva;
  return isFound ? found->second : kUnresolved;
}

}  // namespace edgeplus::edge::translation
