// Unit tests for resolution.h and build_relation.h.
#include <cstdio>
#include <vector>
#include "../../src/edge/resolution.h"

using namespace edgeplus;
using namespace edgeplus::resolution;
using edgeplus::edge155::signatures::Entry;
using edgeplus::edge155::signatures::EntryKind;
using edgeplus::edge_build::Relation;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

using Hits = std::vector<uintptr_t>;

constexpr uint32_t kSecondWindowOffset = 0x40;
constexpr uintptr_t kFunction = 0x5000;

// Two windows: the opening, and one 0x40 bytes in.
Entry TwoWindowEntry(EntryKind kind = EntryKind::kFunction) {
  Entry entry{};
  entry.kind = kind;
  entry.functionSize = 200;
  entry.probes[0] = {"48 89 5C 24 08", 0, 0, 1};
  entry.probes[1] = {"E8 ?? ?? ?? ??", kSecondWindowOffset, 0, 1};
  return entry;
}

Entry FirstWindowOnlyEntry(EntryKind kind, uint8_t ordinal, uint8_t total) {
  Entry entry{};
  entry.kind = kind;
  entry.probes[0] = {"48 89 5C 24 08", 0, ordinal, total};
  return entry;
}

bool Anywhere(uintptr_t) {
  return true;
}

constexpr edge_build::Version kPinned{156, 0, 4314, 50};
static_assert(edge_build::Classify({156, 0, 4314, 50}, kPinned) ==
              Relation::kPinned);
static_assert(edge_build::Classify({156, 0, 4314, 62}, kPinned) ==
              Relation::kSamePatchLine);
static_assert(edge_build::Classify({156, 0, 4350, 50}, kPinned) ==
              Relation::kOtherBuild);
static_assert(edge_build::Classify({157, 0, 4314, 50}, kPinned) ==
              Relation::kOtherBuild);
static_assert(IsSizePlausible(200, 240) && !IsSizePlausible(200, 300));
static_assert(
    IsSizePlausible(10, 25) && !IsSizePlausible(10, 30),
    "a tiny function gets a fixed slack, not a percentage of nothing");

void BothWindowsAgreeIsStrongEverywhere() {
  const std::vector<Hits> hits = {{kFunction},
                                  {kFunction + kSecondWindowOffset}};
  for (const Relation relation :
       {Relation::kSamePatchLine, Relation::kOtherBuild}) {
    const Verdict verdict = Decide(TwoWindowEntry(), hits, relation, Anywhere);
    Check(verdict.status == Status::kResolved && verdict.rva == kFunction &&
              verdict.isStrong,
          "two windows naming one address resolve in any build");
  }
}

void OneWindowIsBelievedOnlyWhereItIsSafe() {
  const std::vector<Hits> firstOnly = {{kFunction}, {}};
  Check(Decide(TwoWindowEntry(), firstOnly, Relation::kSamePatchLine, Anywhere)
                .status == Status::kResolved,
        "one window is enough within a patch line");
  Check(Decide(TwoWindowEntry(), firstOnly, Relation::kOtherBuild, Anywhere)
                .status == Status::kRejected,
        "one window is not enough for a function across lines");
  const Entry site = FirstWindowOnlyEntry(EntryKind::kSite, 0, 1);
  const std::vector<Hits> siteHits = {{0x7000}};
  Check(Decide(site, siteHits, Relation::kOtherBuild, Anywhere).status ==
            Status::kResolved,
        "a patch site's single window is believed across lines");
}

void DisagreeingWindowsAreAConflict() {
  const std::vector<Hits> hits = {{kFunction}, {0x9000 + kSecondWindowOffset}};
  Check(Decide(TwoWindowEntry(), hits, Relation::kSamePatchLine, Anywhere)
                .status == Status::kConflict,
        "windows naming different addresses refuse, even within a patch line");
}

void TheGuardDropsImplausibleHits() {
  const std::vector<Hits> hits = {{kFunction},
                                  {kFunction + kSecondWindowOffset}};
  const auto nowhere = [](uintptr_t) {
    return false;
  };
  Check(Decide(TwoWindowEntry(), hits, Relation::kSamePatchLine, nowhere)
                .status == Status::kMissing,
        "a hit that is not a plausible function start counts for nothing");
}

void CopiesAreToldApartByOrdinal() {
  const Entry second = FirstWindowOnlyEntry(EntryKind::kFunction, 1, 3);
  const std::vector<Hits> three = {{0x9000, 0x1000, 0x5000}};
  const Verdict verdict =
      Decide(second, three, Relation::kSamePatchLine, Anywhere);
  Check(verdict.status == Status::kResolved && verdict.rva == 0x5000,
        "the second copy by address is picked from three");
  const std::vector<Hits> two = {{0x9000, 0x1000}};
  Check(Decide(second, two, Relation::kSamePatchLine, Anywhere).status ==
            Status::kMissing,
        "a different number of copies than when signed refuses");
  Check(Decide(second, three, Relation::kOtherBuild, Anywhere).status ==
            Status::kMissing,
        "across lines a copy's rank names nothing: the linker reorders them");
}

void ASecondWindowRescuesAmbiguousOpening() {
  Entry entry = TwoWindowEntry();
  entry.probes[0].total = 1;
  const std::vector<Hits> hits = {{0x1000, kFunction, 0x9000},
                                  {kFunction + kSecondWindowOffset}};
  const Verdict verdict = Decide(entry, hits, Relation::kOtherBuild, Anywhere);
  Check(
      verdict.status == Status::kResolved && verdict.rva == kFunction &&
          verdict.isStrong,
      "the second window picks one of several opening matches, and both agree");
}

void AnEntryWithoutProbesIsUnsigned() {
  const Entry none{};
  const std::vector<Hits> hits = {{}, {}, {}, {}};
  Check(Decide(none, hits, Relation::kSamePatchLine, Anywhere).status ==
            Status::kUnsigned,
        "no probes, no verdict");
}

}  // namespace

int main() {
  try {
    BothWindowsAgreeIsStrongEverywhere();
    OneWindowIsBelievedOnlyWhereItIsSafe();
    DisagreeingWindowsAreAConflict();
    TheGuardDropsImplausibleHits();
    CopiesAreToldApartByOrdinal();
    ASecondWindowRescuesAmbiguousOpening();
    AnEntryWithoutProbesIsUnsigned();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
