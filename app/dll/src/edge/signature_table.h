// signature_table.h — the shape of signatures.g.h (tools/re/New-SignatureTable.ps1):
// how an offsets.h entry is found again in an msedge.dll build it was not
// reversed against. Types only; the matching is signature.h, the verdict is
// resolution.h.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace edgeplus::edge155::signatures {

// The panel's switches, as bits: the one home of the numbers. The generator
// reads them from here. An entry lists every feature that calls it, so a
// feature whose entries do not all resolve turns off by itself and the others
// keep working.
inline constexpr uint16_t kFeatureBitSplit = 1;
inline constexpr uint16_t kFeatureBitTheming = 2;
inline constexpr uint16_t kFeatureBitProtocolHandlers = 4;
inline constexpr uint16_t kFeatureBitEnhancedFind = 8;
inline constexpr uint16_t kFeatureBitWebUiFollowsTheme = 16;
inline constexpr uint16_t kFeatureBitLivePreview = 32;
inline constexpr uint16_t kFeatureBitCyberpunkSkin = 64;
inline constexpr uint16_t kFeatureBitReadingTheme = 128;
inline constexpr uint16_t kEveryFeature = 255;

enum class Feature : uint16_t {
  kSplit = kFeatureBitSplit,
  kTheming = kFeatureBitTheming,
  kProtocolHandlers = kFeatureBitProtocolHandlers,
  kEnhancedFind = kFeatureBitEnhancedFind,
  kWebUiFollowsTheme = kFeatureBitWebUiFollowsTheme,
  kLivePreview = kFeatureBitLivePreview,
  kCyberpunkSkin = kFeatureBitCyberpunkSkin,
  kReadingTheme = kFeatureBitReadingTheme,
};

[[nodiscard]] constexpr uint16_t BitOf(Feature feature) noexcept {
  return static_cast<uint16_t>(feature);
}

enum class EntryKind : uint8_t {
  kFunction,  // the start of a function the exception table lists
  kSite,      // an address inside code: a patch site, cold code
  kData,      // outside code: a vtable, a global, a table
  kCallee,    // a function with no signature of its own, found as the target of
              // a call inside another function that has one
  kInside,  // an address inside code with no usable bytes of its own, found as
            // a fixed distance into a function that has a signature
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

// The code an entry stands on, fingerprinted whole when the table was signed:
// the function itself, the function a site sits in, or the function that
// refers to a datum. On another line of Edge a probe hit says where the code
// is; only an unchanged body says the code still does what it was reversed
// for (the same struct offsets, slots and ids in its instructions), so a
// feature runs there only while every body it stands on is unchanged.
struct Body {
  uint64_t hash;  // FNV-1a over the body, each masked field taken as zero
  const uint32_t* maskOffsets;  // the 4-byte fields a rebuild moves (rel32
                                // branch targets, RIP displacements)
  uint32_t maskCount;
  uint32_t size;    // 0: no body (the entry cannot be checked this way)
  uint32_t offset;  // from the body's start to the entry's anchor (the
                    // entry itself; for data, the function that refers to it)
  bool isListed;    // the body is a function of the exception table, so its
                    // size there must match too
};

[[nodiscard]] constexpr bool HasBody(const Body& body) noexcept {
  return body.size != 0;
}

// Fields are ordered widest first, which is the order a table row is written
// in.
struct Entry {
  uintptr_t pinnedRva;        // the key: the value offsets.h gives the name
  const char* targetPattern;  // callee: how the code it reaches opens
  const char* name;
  std::array<Probe, kProbesPerEntry> probes;
  Body body;
  uint32_t functionSize;   // of the function the probes find (0 for a site)
  uint32_t operandOffset;  // data, callee, inside: bytes from that function's
                           // start to the displacement that addresses the entry
                           // (a RIP displacement, or the 4 bytes of a call/jmp
                           // rel32), or to the address itself
  uint32_t targetSize;  // callee: the size of the function it reaches (0 when
                        // the image's exception table does not list it: a
                        // leaf thunk)
  int32_t dataDelta;    // data: how far the entry sits from the address the
                        // displacement names, for data nothing in code refers
                        // to but whose neighbour is (the icons of one file)
  uint16_t features;    // Feature bits that use the entry
  EntryKind kind;
  uint8_t operandExtra;  // data: immediate bytes after that displacement
};

}  // namespace edgeplus::edge155::signatures
