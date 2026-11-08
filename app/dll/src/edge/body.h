// body.h — whether the code an entry stands on is still the code it was
// reversed from: its body (signature_table.h) hashed the way the generator
// hashed it (tools/re/Signatures.cs, BodyHash), every moving 4-byte field taken
// as zero. Pure: the bytes come in, a yes or no goes out.
#pragma once
#include <cstdint>
#include <span>
#include "edge/signature_table.h"

namespace edgeplus::body {

namespace signatures = ::edgeplus::edge155::signatures;

inline constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
inline constexpr uint64_t kFnvPrime = 1099511628211ULL;
inline constexpr uint32_t kMaskedFieldSize = 4;

// FNV-1a over `code`, the kMaskedFieldSize bytes at each of `maskOffsets`
// taken as zero. The offsets are ascending (the generator writes them in
// instruction order) and never overlap.
[[nodiscard]] inline uint64_t Hash(std::span<const uint8_t> code,
                                   std::span<const uint32_t> maskOffsets) {
  uint64_t hash = kFnvOffsetBasis;
  size_t nextMask = 0;
  for (size_t position = 0; position < code.size(); position++) {
    while (nextMask < maskOffsets.size() &&
           maskOffsets[nextMask] + kMaskedFieldSize <= position) {
      nextMask++;
    }
    const bool isMasked = nextMask < maskOffsets.size() &&
                          position >= maskOffsets[nextMask] &&
                          position < maskOffsets[nextMask] + kMaskedFieldSize;
    const uint8_t value = isMasked ? 0 : code[position];
    hash ^= value;
    hash *= kFnvPrime;
  }
  return hash;
}

// Whether `code` (exactly the body's size, read from where the body starts in
// the running build) is the body that was signed.
[[nodiscard]] inline bool IsUnchanged(const signatures::Body& body,
                                      std::span<const uint8_t> code) {
  const bool hasBody = signatures::HasBody(body);
  if (!hasBody) {
    return false;
  }
  const bool isWholeBody = code.size() == body.size;
  if (!isWholeBody) {
    return false;
  }
  const std::span<const uint32_t> mask(body.maskOffsets, body.maskCount);
  return Hash(code, mask) == body.hash;
}

}  // namespace edgeplus::body
