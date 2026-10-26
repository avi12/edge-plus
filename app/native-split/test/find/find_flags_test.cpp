// Unit tests for find_flags.h.
#include <cstdio>
#include <string>
#include "../../src/find/find_flags.h"
#include "../../src/find/find_text.h"

using namespace edgeplus::find_flags;

namespace {

struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

static_assert(IsDefaultOn(Flag::kMultiline) && !IsDefaultOn(Flag::kSticky));

void CheckTable() {
  std::string letters;
  for (const FlagInfo& info : kFlags) {
    letters.push_back(info.letter);
    Check(edgeplus::find_text::AreRegExpFlags(std::string(1, info.letter)),
          "every offered flag is one the typed form takes");
    const std::string title = SwitchTitle(info);
    const std::string ending = std::string(" (") + info.letter + ")";
    const bool isEndingWithLetter =
        title.size() > ending.size() && title.ends_with(ending);
    Check(isEndingWithLetter, "a title ends with the flag's letter");
    Check(title.size() > std::string(info.title).size(),
          "a title is indented under RegExp");
  }
  Check(letters == "dimsuvy",
        "the table is in JavaScript's canonical order, without g");
  Check(SwitchTitle(kFlags[1]) ==
            std::string(kSubGroupIndent) + "Ignore case (i)",
        "the words keep Edge's tone");
}

void CheckDefaults() {
  const FlagSet start = DefaultFlags();
  Check(Letters(start) == "m",
        "a bar starts as the search always behaved: ^ and $ at line edges");
  Check(IsDefaultOn(Flag::kMultiline) && !IsDefaultOn(Flag::kIgnoreCase),
        "only m starts on, so a RegExp is case sensitive");
}

void CheckLetters() {
  FlagSet set;
  Check(Letters(set).empty(), "no switch on, no letters");
  SetOn(&set, Flag::kSticky, true);
  SetOn(&set, Flag::kIgnoreCase, true);
  SetOn(&set, Flag::kIndices, true);
  Check(Letters(set) == "diy",
        "letters follow the canonical order, not the click order");
  SetOn(&set, Flag::kIgnoreCase, false);
  Check(Letters(set) == "dy", "a switch turned off drops its letter");
  Check(IsOn(set, Flag::kSticky) && !IsOn(set, Flag::kIgnoreCase),
        "IsOn reads back");
}

void CheckExclusive() {
  FlagSet set;
  SetOn(&set, Flag::kUnicode, true);
  SetOn(&set, Flag::kUnicodeSets, true);
  Check(Letters(set) == "v", "turning v on turns u off");
  SetOn(&set, Flag::kUnicode, true);
  Check(Letters(set) == "u", "turning u on turns v off");
  SetOn(&set, Flag::kUnicode, false);
  Check(Letters(set).empty(), "turning u off leaves v off");
  Flag other = Flag::kIndices;
  Check(HasExclusive(Flag::kUnicode, &other) && other == Flag::kUnicodeSets,
        "u excludes v");
  Check(HasExclusive(Flag::kUnicodeSets, &other) && other == Flag::kUnicode,
        "v excludes u");
  Check(!HasExclusive(Flag::kIgnoreCase, &other), "i excludes nothing");
}

void CheckMerge() {
  const FlagSet none;
  Check(Merge("", none) == "g", "nothing typed, nothing on: g alone");
  Check(Merge("i", none) == "gi", "typed flags join g");
  FlagSet ignoring;
  SetOn(&ignoring, Flag::kIgnoreCase, true);
  Check(Merge("", ignoring) == "gi", "a switch alone, with g");
  Check(Merge("i", ignoring) == "gi",
        "typed and switched on the same flag count once");
  Check(Merge("s", ignoring) == "gis", "typed flags OR with the switches");
  Check(Merge("yg", ignoring) == "giy", "g is typed or not, once, canonical");
  Check(Merge("ss", none) == "gs", "a repeated typed letter counts once");
  Check(Merge("x", none) == "g", "a letter that is not a flag is dropped");
  Check(Merge("d", none) == "dg", "d sorts before g");
  FlagSet unicode;
  SetOn(&unicode, Flag::kUnicode, true);
  Check(Merge("v", unicode) == "guv",
        "u and v together are left for the page's RegExp to refuse");
  Check(Merge("", DefaultFlags()) == "gm", "the default switches");
}

void CheckScriptFlags() {
  using edgeplus::find_text::ScriptFlags;
  Check(ScriptFlags("") == "gm",
        "a typed /pattern/ is every match, line edges");
  Check(ScriptFlags("s") == "gms",
        "typed flags join g and m in canonical order");
  Check(ScriptFlags("iu") == "gimu", "i and u");
  Check(ScriptFlags("gm") == "gm", "g and m typed are not doubled");
}

void CheckDecoration() {
  using edgeplus::find_text::RegExpDecoration;
  Check(RegExpDecoration("pric", DefaultFlags()).prefix == "/",
        "the pattern opens with a slash");
  Check(RegExpDecoration("pric", DefaultFlags()).suffix == "/gm",
        "an untouched bar closes with the default flags");
  Check(RegExpDecoration("(?i)pric", DefaultFlags()).suffix == "/gim",
        "a typed (?i) shows as i");
  FlagSet ignoreCaseOnly;
  SetOn(&ignoreCaseOnly, Flag::kIgnoreCase, true);
  Check(RegExpDecoration("", ignoreCaseOnly).suffix == "/gi",
        "the switches alone, with Multiline off, even for an empty box");
  FlagSet sticky = DefaultFlags();
  SetOn(&sticky, Flag::kSticky, true);
  SetOn(&sticky, Flag::kIndices, true);
  Check(RegExpDecoration("a/b", sticky).suffix == "/dgmy",
        "canonical order, and a slash in the pattern changes nothing");
}

}  // namespace

int main() {
  try {
    CheckTable();
    CheckDefaults();
    CheckLetters();
    CheckExclusive();
    CheckMerge();
    CheckScriptFlags();
    CheckDecoration();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
