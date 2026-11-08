// Unit tests for signature.h.
#include <cstdio>
#include <string>
#include <vector>
#include "../../src/edge/signature.h"

using namespace edgeplus::signature;

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

Pattern Must(const char* text) {
  const std::optional<Pattern> pattern = Parse(text);
  Check(pattern.has_value(), "the pattern parses");
  return *pattern;
}

void ParseAndFormatRoundTrip() {
  const Pattern pattern = Must("48 8b ?? ?? 05 e8");
  Check(pattern.bytes.size() == 6, "six bytes");
  Check(pattern.mask[2] == kWildcard && pattern.mask[3] == kWildcard,
        "?? is a wildcard");
  Check(pattern.mask[0] == kLiteral && pattern.bytes[1] == 0x8b,
        "hex is read in either case");
  Check(Format(pattern) == "48 8B ?? ?? 05 E8", "format writes upper case");
  Check(Must("48 ? 05").mask[1] == kWildcard, "a single ? is a wildcard too");
  Check(!Parse("").has_value(), "empty is not a pattern");
  Check(!Parse("48 8").has_value(), "half a byte is not a pattern");
  Check(!Parse("48 zz").has_value(), "a non-hex byte is not a pattern");
}

void MaskHidesTheMovingBytes() {
  // call rel32: E8 plus four displacement bytes.
  const std::vector<uint8_t> call = {0xe8, 0x10, 0x20, 0x30, 0x40, 0xc3};
  const Wildcard displacement{1, 4};
  const Pattern pattern =
      Mask(call, std::span<const Wildcard>(&displacement, 1));
  Check(Format(pattern) == "E8 ?? ?? ?? ?? C3", "the displacement is hidden");
  const Wildcard pastTheEnd{5, 10};
  const Pattern clipped = Mask(call, std::span<const Wildcard>(&pastTheEnd, 1));
  Check(Format(clipped) == "E8 10 20 30 40 ??",
        "a range past the end is clipped");
}

void LongestRunIsTheScanKey() {
  const Pattern pattern = Must("48 ?? 01 02 03 ?? 05 06 07 08 09 ?? 0a");
  const Run run = LongestLiteralRun(pattern);
  Check(run.offset == 6 && run.length == 5, "the longest literal run is found");
  Check(LongestLiteralRun(Must("?? ?? ??")).length == 0,
        "all wildcards: no run");
}

void FindAllHonoursTheMask() {
  const std::vector<uint8_t> haystack = {
      0x00, 0x55, 0x48, 0x8b, 0x05, 0x11, 0x22, 0x33, 0x44, 0xc3, 0x55,
      0x48, 0x8b, 0x05, 0x99, 0x88, 0x77, 0x66, 0xc3, 0x55, 0x48, 0x8b};
  const Pattern pattern = Must("55 48 8b 05 ?? ?? ?? ?? c3");
  const std::vector<size_t> hits = FindAll(haystack, pattern, 8);
  Check(hits.size() == 2 && hits[0] == 1 && hits[1] == 10,
        "both displacements match the wildcards");
  Check(FindAll(haystack, pattern, 1).size() == 1, "the limit caps the hits");
  Check(FindAll(haystack, Must("55 48 8b 05 ?? ?? ?? ?? c4"), 8).empty(),
        "a literal byte that differs matches nowhere");
  Check(FindAll(haystack, Must("48 8b ?? ?? ?? ?? ?? ??"), 8).empty(),
        "a pattern with no run of four literals is never scanned for");
}

void MultiScannerAgreesWithFindAll() {
  std::vector<uint8_t> haystack(4096, 0xcc);
  const std::vector<uint8_t> first = {0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec,
                                      0x20, 0xe8, 0x01, 0x02, 0x03, 0x04, 0x5d};
  const std::vector<uint8_t> second = {0x41, 0x56, 0x56, 0x57, 0x53, 0x48,
                                       0x83, 0xec, 0x28, 0x48, 0x8b, 0x05,
                                       0xaa, 0xbb, 0xcc, 0xdd};
  std::copy(first.begin(), first.end(), haystack.begin() + 100);
  std::copy(first.begin(), first.end(), haystack.begin() + 700);
  std::copy(second.begin(), second.end(), haystack.begin() + 1000);
  std::copy(second.begin(), second.begin() + 8, haystack.begin() + 3000);

  MultiScanner scanner;
  const size_t firstId =
      scanner.Add(Must("55 48 89 e5 48 83 ec 20 e8 ?? ?? ?? ?? 5d"));
  const size_t secondId =
      scanner.Add(Must("41 56 56 57 53 48 83 ec 28 48 8b 05 ?? ?? ?? ??"));
  const size_t shortId = scanner.Add(Must("48 ?? ?? ?? ?? 83 ?? 20"));
  scanner.Scan(haystack);
  Check(scanner.HitsOf(firstId) == std::vector<size_t>({100, 700}),
        "a pattern found twice reports both");
  Check(scanner.HitsOf(secondId) == std::vector<size_t>({1000}),
        "the truncated copy at 3000 does not match the whole pattern");
  Check(scanner.HitsOf(shortId).empty(), "an unscannable pattern has no hits");
  Check(FindAll(haystack, Must("55 48 89 e5 48 83 ec 20 e8 ?? ?? ?? ?? 5d"),
                8) == scanner.HitsOf(firstId),
        "the one-pass scan finds what a single search finds");
}

void ScannerStopsAtTheEdges() {
  const std::vector<uint8_t> haystack = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  MultiScanner scanner;
  const size_t atStart = scanner.Add(Must("01 02 03 04"));
  const size_t atEnd = scanner.Add(Must("03 04 05 06"));
  const size_t overrun = scanner.Add(Must("05 06 07 08"));
  const size_t startsBefore = scanner.Add(Must("?? 01 02 03 04"));
  scanner.Scan(haystack);
  Check(scanner.HitsOf(atStart) == std::vector<size_t>({0}),
        "a match at the start");
  Check(scanner.HitsOf(atEnd) == std::vector<size_t>({2}),
        "a match at the end");
  Check(scanner.HitsOf(overrun).empty(), "a pattern running past the end");
  Check(scanner.HitsOf(startsBefore).empty(),
        "a pattern whose first byte would lie before the haystack");
}

}  // namespace

int main() {
  try {
    ParseAndFormatRoundTrip();
    MaskHidesTheMovingBytes();
    LongestRunIsTheScanKey();
    FindAllHonoursTheMask();
    MultiScannerAgreesWithFindAll();
    ScannerStopsAtTheEdges();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
