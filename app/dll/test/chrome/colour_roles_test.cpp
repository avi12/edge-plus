// Unit tests for colour_roles.h: the table that says which Edge colours the
// Cyberpunk skin takes over.
#include <cstdio>
#include <exception>
#include <set>
#include <string>
#include "../../src/chrome/colours/colour_roles.h"

using namespace edgeplus::colour_roles;
using edgeplus::cyberpunk_palette::Role;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

void Check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

void RunTableChecks() {
  std::set<std::string> seen;
  for (const Entry& entry : kEntries) {
    const std::string name(entry.name);
    Check(name.starts_with("kColor"),
          "every name is one of Edge's `kColor` enumerators");
    Check(seen.insert(name).second, "no colour is given two roles");
    Check(entry.role != Role::kNone, "every colour has a role");
    Check(name.find("kColorWeb") == std::string::npos,
          "a web page's own controls are left alone");
  }
  Check(seen.size() > 100,
        "the skin takes over the browser, not a corner of it");
}

// What the tab skin owns (hooks.h) must not also be a role here.
void RunTabsStayWithTheTabSkin() {
  for (const Entry& entry : kEntries) {
    const std::string name(entry.name);
    const bool isTabColour =
        name.starts_with("kColorTab") && !name.starts_with("kColorTable");
    Check(!isTabColour, "the tab strip's colours are the tab skin's");
  }
}

}  // namespace

int main() {
  try {
    RunTableChecks();
    RunTabsStayWithTheTabSkin();
  } catch (const CheckFailed&) {
    return 1;
  } catch (const std::exception&) {
    // The table checks allocate strings.
    return 1;
  }
  std::printf("colour_roles_test: all checks passed\n");
  return 0;
}
