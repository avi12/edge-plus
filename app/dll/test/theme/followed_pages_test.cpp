// Unit tests for followed_pages.h.
#include <cstdio>
#include "../../src/theme/followed_pages.h"

using edgeplus::followed_pages::IsAnyOn;
using edgeplus::followed_pages::IsRemixPage;
using edgeplus::followed_pages::Switches;

namespace {
constexpr Switches kWebUiOnly{true, false};
constexpr Switches kRemixOnly{false, true};
constexpr Switches kBoth{true, true};

bool Follows(const char* url) {
  return edgeplus::followed_pages::Follows(url, kWebUiOnly);
}
}  // namespace

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
  Check(!Follows("edge://blueprint"), "remix pages need their own switch");
  Check(!Follows("edge://remix/"), "the manager needs its own switch");
}

void RunRemixChecks() {
  using edgeplus::followed_pages::Follows;
  Check(Follows("edge://blueprint", kRemixOnly), "the bubble");
  Check(Follows("edge://blueprint/?x=1#editor", kRemixOnly), "bubble route");
  Check(Follows("edge://remix", kRemixOnly), "the manager");
  Check(Follows("edge://remix/", kBoth), "the manager, both on");
  Check(!Follows("edge://blueprintfoo", kRemixOnly), "another host");
  Check(!Follows("edge://remixes/", kRemixOnly), "another host, remixes");
  Check(!Follows("edge://settings", kRemixOnly), "settings needs its switch");
  Check(Follows("edge://settings", kBoth), "settings, both on");
  Check(!Follows("edge://blueprint", Switches{}), "all off");
  Check(!IsAnyOn(Switches{}), "none on");
  Check(IsAnyOn(kRemixOnly), "remix on");
  Check(IsRemixPage("edge://blueprint/") && IsRemixPage("edge://remix") &&
            !IsRemixPage("edge://settings") && !IsRemixPage("edge://remixfoo"),
        "the skin's pages are the blueprint and the remix manager only");
}

}  // namespace

int main() {
  try {
    RunChecks();
    RunRemixChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
