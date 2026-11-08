// resolution.h — what a table entry's signature hits add up to. Pure: the hits
// come in as numbers, one verdict goes out.
//
// A probe is one signature of the entry's function; the opening of the function
// is the first window and one further in is the second. An address that both
// windows name is strong evidence; one window alone is weak, believed only where
// measurement shows it safe. Measured on the 155.62 / 156.42 / 156.50 images:
// within a patch line (156.42 <-> 156.50) every function a signature finds is
// right; across lines, a function a single window names was wrong for 2 of 41,
// while an address both windows name was wrong for 1 of 104 and a patch site's
// single window was never wrong (docs/future-proofing.md).
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <vector>
#include "edge/build_relation.h"
#include "edge/signature_table.h"

namespace edgeplus::resolution {

namespace signatures = ::edgeplus::edge155::signatures;

enum class Status {
  kResolved,
  kUnsigned,  // the entry has no signature at all
  kMissing,   // no signature matched where the guard allows
  kConflict,  // signatures named different addresses
  kRejected,  // one window named an address, and that is too little here
};

struct Verdict {
  Status status = Status::kUnsigned;
  uintptr_t rva = 0;
  bool isStrong = false;
};

// A function of about the size it had when it was signed.
inline constexpr uint32_t kSizeTolerancePercent = 25;
inline constexpr uint32_t kSizeSlackBytes = 16;
inline constexpr uint32_t kPercent = 100;

[[nodiscard]] constexpr bool IsSizePlausible(uint32_t expected,
                                             uint32_t actual) noexcept {
  const uint32_t difference =
      expected > actual ? expected - actual : actual - expected;
  return difference <=
         expected * kSizeTolerancePercent / kPercent + kSizeSlackBytes;
}

// What one window may decide alone.
[[nodiscard]] constexpr bool IsWeakAcceptable(
    signatures::EntryKind kind, edge_build::Relation relation) noexcept {
  const bool isPatchLine = relation != edge_build::Relation::kOtherBuild;
  const bool isSite = kind == signatures::EntryKind::kSite;
  return isPatchLine || isSite;
}

// hitsPerProbe[i]: the addresses (RVAs) probe i of the entry matched. isPlausible
// says whether an address is somewhere the entry could be (a function start of
// about the right size; for a site, any code).
[[nodiscard]] inline Verdict Decide(
    const signatures::Entry& entry,
    std::span<const std::vector<uintptr_t>> hitsPerProbe,
    edge_build::Relation relation,
    const std::function<bool(uintptr_t)>& isPlausible) {
  constexpr uint8_t kFirstWindow = 1;
  constexpr uint8_t kSecondWindow = 2;
  constexpr uint8_t kBothWindows = kFirstWindow | kSecondWindow;

  std::vector<uintptr_t> firstWindowHits;
  bool isSigned = false;
  for (size_t i = 0; i < entry.probes.size(); i++) {
    const signatures::Probe& probe = entry.probes[i];
    if (signatures::IsEmpty(probe)) {
      continue;
    }
    isSigned = true;
    const bool isFirstWindow = probe.offset == 0;
    if (isFirstWindow) {
      firstWindowHits.insert(firstWindowHits.end(), hitsPerProbe[i].begin(),
                             hitsPerProbe[i].end());
    }
  }
  if (!isSigned) {
    return {Status::kUnsigned, 0, false};
  }

  std::map<uintptr_t, uint8_t> windowsByCandidate;
  for (size_t i = 0; i < entry.probes.size(); i++) {
    const signatures::Probe& probe = entry.probes[i];
    if (signatures::IsEmpty(probe)) {
      continue;
    }
    std::vector<uintptr_t> hits(hitsPerProbe[i].begin(), hitsPerProbe[i].end());
    std::sort(hits.begin(), hits.end());
    const bool isFirstWindow = probe.offset == 0;
    if (isFirstWindow) {
      const bool hasExpectedCount = hits.size() == probe.total;
      if (!hasExpectedCount) {
        continue;
      }
      const uintptr_t candidate = hits[probe.ordinal];
      if (isPlausible(candidate)) {
        windowsByCandidate[candidate] |= kFirstWindow;
      }
      continue;
    }
    const bool isUnique = hits.size() == 1;
    if (!isUnique) {
      continue;
    }
    const bool isBeforeFunction = hits[0] < probe.offset;
    if (isBeforeFunction) {
      continue;
    }
    const uintptr_t candidate = hits[0] - probe.offset;
    if (!isPlausible(candidate)) {
      continue;
    }
    windowsByCandidate[candidate] |= kSecondWindow;
    const bool isAlsoFirstWindowHit =
        std::find(firstWindowHits.begin(), firstWindowHits.end(), candidate) !=
        firstWindowHits.end();
    if (isAlsoFirstWindowHit) {
      windowsByCandidate[candidate] |= kFirstWindow;
    }
  }

  if (windowsByCandidate.empty()) {
    return {Status::kMissing, 0, false};
  }
  if (windowsByCandidate.size() > 1) {
    return {Status::kConflict, 0, false};
  }
  const auto [rva, windows] = *windowsByCandidate.begin();
  const bool isStrong = windows == kBothWindows;
  if (isStrong || IsWeakAcceptable(entry.kind, relation)) {
    return {Status::kResolved, rva, isStrong};
  }
  return {Status::kRejected, 0, false};
}

}  // namespace edgeplus::resolution
