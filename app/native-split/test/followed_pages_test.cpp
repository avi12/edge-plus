// Unit tests for followed_pages.h.
#include <cstdio>
#include "../src/followed_pages.h"

using edgeplus::followed_pages::Follows;

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
  Check(Follows("edge://settings"), "settings, bare");
  Check(Follows("edge://settings/profiles"), "a settings subpage");
  Check(Follows("edge://settings/?search=theme"), "settings with a query");
  Check(Follows("edge://settings#top"), "settings with a fragment");
  Check(Follows("edge://extensions/"), "extensions");
  Check(Follows("edge://extensions/shortcuts"), "an extensions subpage");
  Check(!Follows("edge://settingsfoo"), "another host that starts the same");
  Check(!Follows("edge://extensions-internals/"), "extensions-internals");
  Check(!Follows("edge://history/"), "other Edge pages keep Edge's choice");
  Check(!Follows("https://example.com/edge://settings"), "a web page");
  Check(!Follows("https://settings.example.com/"), "a web host");
  Check(!Follows(""), "no URL yet");
  Check(!Follows("edge://"), "the scheme alone");
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
