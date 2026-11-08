// signature.h — masked-byte patterns and the one-pass scanner that finds many of
// them in a code section. Pure: bytes in, positions out, no Edge, no Win32.
//
// A pattern is the opening of a function with the bytes a rebuild moves
// (call/jump targets, RIP-relative displacements) wildcarded; msedge.dll's code
// carries no base relocations, so those are the only ones. tools/re/
// make_signatures.py writes patterns as text ("48 8B ?? ?? ?? ??"), Parse reads
// them back, and Mask builds one from raw bytes and the ranges to hide.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace edgeplus::signature {

inline constexpr uint8_t kLiteral = 0xff;
inline constexpr uint8_t kWildcard = 0x00;
inline constexpr char kWildcardToken = '?';
// The scanner indexes a pattern by its longest literal run; a shorter one
// would match too often to be worth scanning for.
inline constexpr size_t kKeyLength = 4;
inline constexpr size_t kMaximumHitsPerPattern = 16;
inline constexpr int kHexDigitsPerByte = 2;
inline constexpr int kBitsPerHexDigit = 4;
inline constexpr int kHexLetterBase = 10;

struct Pattern {
  std::vector<uint8_t> bytes;
  std::vector<uint8_t> mask;  // kLiteral where the byte must match
};

struct Wildcard {
  size_t offset;
  size_t length;
};

struct Run {
  size_t offset = 0;
  size_t length = 0;
};

namespace detail {

[[nodiscard]] constexpr int HexValue(char digit) noexcept {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  }
  if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + kHexLetterBase;
  }
  if (digit >= 'A' && digit <= 'F') {
    return digit - 'A' + kHexLetterBase;
  }
  return -1;
}

[[nodiscard]] constexpr bool IsSeparator(char letter) noexcept {
  return letter == ' ' || letter == '\t';
}

}  // namespace detail

// "48 8B ?? 05": two hex digits per literal byte, ?? (or ?) per wildcard,
// separated by spaces. nullopt for anything else, and for an empty pattern.
[[nodiscard]] inline std::optional<Pattern> Parse(std::string_view text) {
  Pattern pattern;
  size_t position = 0;
  while (position < text.size()) {
    const char letter = text[position];
    if (detail::IsSeparator(letter)) {
      position++;
      continue;
    }
    const bool isWildcard = letter == kWildcardToken;
    if (isWildcard) {
      position++;
      const bool isDoubleToken =
          position < text.size() && text[position] == kWildcardToken;
      if (isDoubleToken) {
        position++;
      }
      pattern.bytes.push_back(0);
      pattern.mask.push_back(kWildcard);
      continue;
    }
    const bool hasTwoDigits = position + 1 < text.size();
    if (!hasTwoDigits) {
      return std::nullopt;
    }
    const int high = detail::HexValue(letter);
    const int low = detail::HexValue(text[position + 1]);
    const bool isHexByte = high >= 0 && low >= 0;
    if (!isHexByte) {
      return std::nullopt;
    }
    pattern.bytes.push_back(
        static_cast<uint8_t>((high << kBitsPerHexDigit) | low));
    pattern.mask.push_back(kLiteral);
    position += kHexDigitsPerByte;
  }
  if (pattern.bytes.empty()) {
    return std::nullopt;
  }
  return pattern;
}

[[nodiscard]] inline std::string Format(const Pattern& pattern) {
  static constexpr char kDigits[] = "0123456789ABCDEF";
  std::string text;
  for (size_t i = 0; i < pattern.bytes.size(); i++) {
    if (i != 0) {
      text.push_back(' ');
    }
    const bool isWildcard = pattern.mask[i] == kWildcard;
    if (isWildcard) {
      text.append("??");
      continue;
    }
    text.push_back(kDigits[pattern.bytes[i] >> kBitsPerHexDigit]);
    text.push_back(kDigits[pattern.bytes[i] & 0xf]);
  }
  return text;
}

// The mask builder: raw instruction bytes and the (offset, length) ranges a
// rebuild moves become a pattern. Ranges past the end are clipped.
[[nodiscard]] inline Pattern Mask(std::span<const uint8_t> bytes,
                                  std::span<const Wildcard> wildcards) {
  Pattern pattern;
  pattern.bytes.assign(bytes.begin(), bytes.end());
  pattern.mask.assign(bytes.size(), kLiteral);
  for (const Wildcard& wildcard : wildcards) {
    const size_t end =
        std::min(wildcard.offset + wildcard.length, bytes.size());
    for (size_t i = wildcard.offset; i < end; i++) {
      pattern.mask[i] = kWildcard;
      pattern.bytes[i] = 0;
    }
  }
  return pattern;
}

[[nodiscard]] inline Run LongestLiteralRun(const Pattern& pattern) {
  Run best;
  size_t start = 0;
  bool isInRun = false;
  for (size_t i = 0; i < pattern.mask.size(); i++) {
    const bool isLiteral = pattern.mask[i] == kLiteral;
    if (!isLiteral) {
      isInRun = false;
      continue;
    }
    if (!isInRun) {
      start = i;
      isInRun = true;
    }
    const size_t length = i - start + 1;
    if (length > best.length) {
      best = {start, length};
    }
  }
  return best;
}

[[nodiscard]] inline bool MatchesAt(std::span<const uint8_t> haystack,
                                    size_t position, const Pattern& pattern) {
  const bool isInside = position <= haystack.size() &&
                        pattern.bytes.size() <= haystack.size() - position;
  if (!isInside) {
    return false;
  }
  for (size_t i = 0; i < pattern.bytes.size(); i++) {
    const bool isMismatch = pattern.mask[i] == kLiteral &&
                            haystack[position + i] != pattern.bytes[i];
    if (isMismatch) {
      return false;
    }
  }
  return true;
}

// Every place one pattern matches, by position; at most `limit` of them.
[[nodiscard]] inline std::vector<size_t> FindAll(
    std::span<const uint8_t> haystack, const Pattern& pattern, size_t limit) {
  std::vector<size_t> hits;
  const Run run = LongestLiteralRun(pattern);
  const bool isScannable = run.length >= kKeyLength;
  if (!isScannable) {
    return hits;
  }
  for (size_t position = 0; position + pattern.bytes.size() <= haystack.size();
       position++) {
    if (MatchesAt(haystack, position, pattern)) {
      hits.push_back(position);
      if (hits.size() >= limit) {
        break;
      }
    }
  }
  return hits;
}

// Many patterns, one walk over the haystack. Each pattern is filed under the
// first kKeyLength bytes of its longest literal run; a small bit set rejects
// almost every position before any lookup.
class MultiScanner {
 public:
  // The id the hits of this pattern are read back by. A pattern with no
  // literal run of kKeyLength bytes is never found.
  size_t Add(Pattern pattern) {
    const size_t patternId = patterns_.size();
    const Run run = LongestLiteralRun(pattern);
    const bool isScannable = run.length >= kKeyLength;
    if (isScannable) {
      const uint32_t key = KeyAt(pattern.bytes.data() + run.offset);
      byKey_.emplace(key, patternId);
      seen_[Slot(key)] = true;
    }
    runOffsets_.push_back(run.offset);
    patterns_.push_back(std::move(pattern));
    hits_.emplace_back();
    return patternId;
  }

  void Scan(std::span<const uint8_t> haystack) {
    if (haystack.size() < kKeyLength) {
      return;
    }
    const uint8_t* const data = haystack.data();
    const size_t last = haystack.size() - kKeyLength;
    for (size_t position = 0; position <= last; position++) {
      const uint32_t key = KeyAt(data + position);
      if (!seen_[Slot(key)]) {
        continue;
      }
      const auto [first, end] = byKey_.equal_range(key);
      for (auto it = first; it != end; ++it) {
        Record(haystack, position, it->second);
      }
    }
  }

  [[nodiscard]] const std::vector<size_t>& HitsOf(size_t patternId) const {
    return hits_[patternId];
  }

 private:
  static constexpr size_t kSlotBits = 22;
  static constexpr uint32_t kSlotMultiplier = 2654435761u;
  static constexpr size_t kSlotShift = 32 - kSlotBits;

  [[nodiscard]] static uint32_t KeyAt(const uint8_t* bytes) {
    uint32_t key = 0;
    std::memcpy(&key, bytes, kKeyLength);
    return key;
  }

  [[nodiscard]] static size_t Slot(uint32_t key) {
    return static_cast<size_t>((key * kSlotMultiplier) >> kSlotShift);
  }

  void Record(std::span<const uint8_t> haystack, size_t keyPosition,
              size_t patternId) {
    const size_t runOffset = runOffsets_[patternId];
    const bool isBeforeStart = keyPosition < runOffset;
    if (isBeforeStart) {
      return;
    }
    const size_t start = keyPosition - runOffset;
    const bool isFull = hits_[patternId].size() >= kMaximumHitsPerPattern;
    if (isFull) {
      return;
    }
    if (MatchesAt(haystack, start, patterns_[patternId])) {
      hits_[patternId].push_back(start);
    }
  }

  std::vector<Pattern> patterns_;
  std::vector<size_t> runOffsets_;
  std::vector<std::vector<size_t>> hits_;
  std::unordered_multimap<uint32_t, size_t> byKey_;
  std::vector<bool> seen_ = std::vector<bool>(size_t{1} << kSlotBits);
};

}  // namespace edgeplus::signature
