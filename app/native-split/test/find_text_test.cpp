// Unit tests for the pure text side of the find bar (find_text.h). No Edge/Win32
// deps. Run via test\run.cmd.
#include <cstdio>
#include <string>
#include "../src/find_text.h"

using namespace fixedge::find_text;

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

// The fields joined by the field separator (a hex escape would swallow the
// digit after it).
std::string Fields(std::initializer_list<std::string> fields) {
  std::string joined;
  bool isFirst = true;
  for (const std::string& field : fields) {
    if (!isFirst) {
      joined += kFieldSeparator;
    }
    isFirst = false;
    joined += field;
  }
  return joined;
}

void CheckPatternShape() {
  Check(CheckRegex("") == PatternProblem::kEmpty, "empty is nothing to search");
  Check(CheckRegex("\\d+\\.\\d+") == PatternProblem::kNone, "plain pattern");
  Check(CheckRegex("[\\w.-]+@[a-z]+") == PatternProblem::kNone, "classes");
  Check(CheckRegex("(foo|bar)baz") == PatternProblem::kNone, "group");
  Check(CheckRegex("(a+)b") == PatternProblem::kNone,
        "repeat inside, none outside");
  Check(CheckRegex("(a+\\d)+") == PatternProblem::kNone,
        "an escape after the repeat ends it");
  Check(CheckRegex("(a{2})+") == PatternProblem::kNone,
        "a counted repeat is finite");
  Check(CheckRegex("[(]") == PatternProblem::kNone,
        "a paren in a class is a letter");
  Check(CheckRegex("a\\(") == PatternProblem::kNone, "an escaped paren");
  Check(CheckRegex("(foo") == PatternProblem::kUnbalanced, "open group");
  Check(CheckRegex("foo)") == PatternProblem::kUnbalanced, "stray close");
  Check(CheckRegex("[abc") == PatternProblem::kUnbalanced, "open class");
  Check(CheckRegex("abc\\") == PatternProblem::kDanglingEscape,
        "lone backslash");
  Check(CheckRegex("(a+)+") == PatternProblem::kNestedRepeat, "(a+)+");
  Check(CheckRegex("(a*)*b") == PatternProblem::kNestedRepeat, "(a*)*");
  Check(CheckRegex("(a+?)*") == PatternProblem::kNestedRepeat,
        "a lazy repeat is still a repeat");
  Check(CheckRegex(std::string(kMaxPatternChars + 1, 'a')) ==
            PatternProblem::kTooLong,
        "too long");
  Check(std::string(ProblemText(PatternProblem::kNestedRepeat)).size() > 0,
        "a problem has words");
}

void CheckBuildSearch() {
  Options options;
  Search search = BuildSearch("a.b", options);
  Check(search.problem == PatternProblem::kNone && search.source == "a\\.b" &&
            search.flags == "gim",
        "text mode escapes and ignores case");
  options.isCaseSensitive = true;
  search = BuildSearch("x", options);
  Check(search.flags == "gm", "case sensitive drops i");
  options.mode = Mode::kRegex;
  search = BuildSearch("a.b", options);
  Check(search.source == "a.b", "regex mode keeps the pattern");
  options.isWholeWord = true;
  search = BuildSearch("cat|dog", options);
  Check(search.source == "(?<!\\w)(?:cat|dog)(?!\\w)",
        "whole word wraps the alternatives together");
  search = BuildSearch("(a", options);
  Check(search.problem == PatternProblem::kUnbalanced && search.source.empty(),
        "a bad pattern builds nothing");
  options.mode = Mode::kText;
  search = BuildSearch("(a", options);
  Check(search.problem == PatternProblem::kNone,
        "text mode takes brackets literally");
  search = BuildSearch("", options);
  Check(search.problem == PatternProblem::kEmpty, "empty text");
  Check(EscapeLiteral("a-b/c") == "a\\-b\\/c", "escapes - and /");
}

void CheckStepping() {
  Check(StepMatch(kNoMatch, 5, Step::kNext) == 0, "first next");
  Check(StepMatch(kNoMatch, 5, Step::kPrevious) == 4, "first previous");
  Check(StepMatch(4, 5, Step::kNext) == 0, "next wraps");
  Check(StepMatch(0, 5, Step::kPrevious) == 4, "previous wraps");
  Check(StepMatch(2, 5, Step::kNext) == 3, "next");
  Check(StepMatch(2, 5, Step::kPrevious) == 1, "previous");
  Check(StepMatch(0, 0, Step::kNext) == kNoMatch, "nothing to step through");
  Check(StepMatch(9, 5, Step::kNext) == 0, "a stale index starts over");
  Check(MatchLabel(2, 12, false) == "3 of 12", "label");
  Check(MatchLabel(kNoMatch, 12, false) == "12 results",
        "label before stepping");
  Check(MatchLabel(kNoMatch, 1, false) == "1 result", "one result");
  Check(MatchLabel(0, 5000, true) == "1 of 5000+", "capped label");
  Check(MatchLabel(kNoMatch, 0, false) == "No results", "no results");
}

void CheckSearchResults() {
  SearchResult result = ParseSearchResult(Fields({"ok", "12", "0"}));
  Check(result.isOk && result.count == 12 && !result.isCapped, "ok");
  result = ParseSearchResult(Fields({"ok", "5000", "1"}));
  Check(result.isOk && result.isCapped, "capped");
  result = ParseSearchResult(Fields({"err", "Invalid regular expression"}));
  Check(!result.isOk && result.error == "Invalid regular expression", "err");
  result = ParseSearchResult("garbage");
  Check(!result.isOk && !result.error.empty(), "unreadable answer");
  result = ParseSearchResult(Fields({"ok", "x", "0"}));
  Check(!result.isOk, "a count that is not a number");
  result = ParseSearchResult("");
  Check(!result.isOk, "empty answer");
}

void CheckReport() {
  Report report = ParseReport(kNotInstalledAnswer);
  Check(!report.isInstalled && report.events.empty(), "not installed");
  report = ParseReport("fixedge-find");
  Check(report.isInstalled && report.events.empty(), "installed, quiet");
  report = ParseReport(
      "fixedge-find\n" + Fields({"query", "regex", "1", "0", "(a|b)"}) +
      "\nnext\nprev\n" + Fields({"ask", "ask", "0", "0", "where is pricing?"}) +
      "\nclose");
  Check(report.isInstalled && report.events.size() == 5, "five events");
  Check(report.events[0].kind == EventKind::kQuery &&
            report.events[0].options.mode == Mode::kRegex &&
            report.events[0].options.isCaseSensitive &&
            !report.events[0].options.isWholeWord &&
            report.events[0].text == "(a|b)",
        "query event");
  Check(report.events[1].kind == EventKind::kNext &&
            report.events[2].kind == EventKind::kPrevious,
        "next and previous");
  Check(report.events[3].kind == EventKind::kAsk &&
            report.events[3].options.mode == Mode::kAsk &&
            report.events[3].text == "where is pricing?",
        "ask event");
  Check(report.events[4].kind == EventKind::kClose, "close");
  report =
      ParseReport("fixedge-find\n" + Fields({"query", "bogus", "0", "0", "x"}) +
                  "\nwhat\n" + Fields({"query", "x"}));
  Check(report.isInstalled && report.events.empty(),
        "lines it cannot read are dropped");
  report =
      ParseReport("fixedge-find\n" + Fields({"query", "text", "0", "1", ""}));
  Check(report.events.size() == 1 && report.events[0].text.empty() &&
            report.events[0].options.isWholeWord,
        "an emptied box is a query too");
}

}  // namespace

int main() {
  try {
    CheckPatternShape();
    CheckBuildSearch();
    CheckStepping();
    CheckSearchResults();
    CheckReport();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
