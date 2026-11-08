// Unit tests for body.h.
#include <array>
#include <cstdio>
#include <vector>
#include "../../src/edge/body.h"

using namespace edgeplus;
using edgeplus::edge155::signatures::Body;

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

// `call rel32; mov eax, [rcx+0x950]; ret`: the call's target moves between
// builds, the field offset is what the body vouches for.
constexpr std::array<uint8_t, 12> kSigned = {
    0xE8, 0x11, 0x22, 0x33, 0x44, 0x8B, 0x81, 0x50, 0x09, 0x00, 0x00, 0xC3};
const uint32_t kMask[] = {1};

Body SignedBody() {
  return Body{body::Hash(kSigned, kMask),
              kMask,
              1,
              static_cast<uint32_t>(kSigned.size()),
              0,
              true};
}

void RunChecks() {
  const Body signedBody = SignedBody();
  Check(body::IsUnchanged(signedBody, kSigned), "the signed code is unchanged");

  std::vector<uint8_t> moved(kSigned.begin(), kSigned.end());
  moved[1] = 0x99;
  moved[4] = 0x01;
  Check(body::IsUnchanged(signedBody, moved),
        "a call target that moved is still the same code");

  std::vector<uint8_t> relaid(kSigned.begin(), kSigned.end());
  relaid[7] = 0x58;
  Check(!body::IsUnchanged(signedBody, relaid),
        "a field offset that moved is changed code");

  std::vector<uint8_t> longer(kSigned.begin(), kSigned.end());
  longer.push_back(0xCC);
  Check(!body::IsUnchanged(signedBody, longer),
        "code of another size is changed code");

  const Body none{};
  Check(!body::IsUnchanged(none, kSigned),
        "an entry with no body is never vouched for");

  const uint32_t noMask[] = {0};
  Check(body::Hash(kSigned, std::span<const uint32_t>(noMask, 0)) !=
            body::Hash(kSigned, kMask),
        "the mask takes part in the hash");
}

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
