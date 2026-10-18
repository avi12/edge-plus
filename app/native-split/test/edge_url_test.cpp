// Unit tests for the Chrome-to-Edge page mapping (edge_url.h). No Edge/Win32
// deps. Run via test\run.cmd.
#include <cstdio>
#include "../src/edge_url.h"

using namespace fixedge::edge_url;

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

void RunChecks() {
  Check(EdgeUrlFor("chrome://settings/ai/gemini") == "edge://settings/ai",
        "Gemini's settings are the AI settings");
  Check(EdgeUrlFor("chrome://password-manager/settings") ==
            "edge://wallet/passwords",
        "the password manager is the wallet's passwords");
  Check(EdgeUrlFor("chrome://settings/addresses") == "edge://settings/profiles",
        "addresses are personal info");
  Check(EdgeUrlFor("chrome://settings/privacy") == "edge://settings/privacy",
        "an unmapped chrome page keeps its path under edge");
  Check(EdgeUrlFor("https://example.com/") == "https://example.com/",
        "a web page is untouched");
  Check(IsOpenable("edge://settings/sidebar") && IsOpenable("https://a.b/"),
        "edge and web pages may be opened");
  Check(!IsOpenable("javascript:alert(1)") && !IsOpenable("file:///c:/x") &&
            !IsOpenable("chrome://settings"),
        "scripts, files and unmapped schemes may not");
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
