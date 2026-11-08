// signature_table.h — the shape of signatures.g.h (tools/re/make_signatures.py):
// how an offsets.h entry is found again in an msedge.dll build it was not
// reversed against. Types only; the matching is signature.h, the verdict is
// resolution.h.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace edgeplus::edge155::signatures {

// The panel's switches, as bits. An entry lists every feature that calls it, so a
// feature whose entries do not all resolve turns off by itself and the others
// keep working. Mirrors FEATURE_BITS in make_signatures.py.
enum class Feature : uint16_t {
  kSplit = 1 << 0,
  kTheming = 1 << 1,
  kProtocolHandlers = 1 << 2,
  kEnhancedFind = 1 << 3,
  kWebUiFollowsTheme = 1 << 4,
  kLivePreview = 1 << 5,
  kTabShape = 1 << 6,
  kReadingTheme = 1 << 7,
};
inline constexpr uint16_t kEveryFeature = 0xff;

[[nodiscard]] constexpr uint16_t BitOf(Feature feature) noexcept {
  return static_cast<uint16_t>(feature);
}

enum class EntryKind : uint8_t {
  kFunction,  // the start of a function the exception table lists
  kSite,      // an address inside code: a patch site, cold code
  kData,      // outside code: a vtable, a global, a table
};

// One masked-byte signature, "48 8B ?? 05 ..." (signature.h parses it).
struct Probe {
  const char* pattern;  // empty when the entry has no probe in this slot
  uint32_t offset;      // bytes from the entry's function start to the match
  uint8_t ordinal;      // the function is this match, by address, of `total`
  uint8_t total;
};

[[nodiscard]] constexpr bool IsEmpty(const Probe& probe) noexcept {
  return probe.pattern == nullptr || probe.pattern[0] == '\0';
}

inline constexpr size_t kProbesPerEntry = 4;

struct Entry {
  uintptr_t pinnedRva;  // the key: the value offsets.h gives the name
  EntryKind kind;
  uint16_t features;       // Feature bits that use the entry
  uint32_t functionSize;   // of the function the probes find (0 for a site)
  uint32_t operandOffset;  // data: bytes from that function's start to the RIP
                           // displacement that addresses the entry
  uint8_t operandExtra;    // data: immediate bytes after that displacement
  std::array<Probe, kProbesPerEntry> probes;
  const char* name;
};

}  // namespace edgeplus::edge155::signatures
