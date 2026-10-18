// Unit tests for the pure text side of the enhanced find (find_text.h). No Edge/Win32
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

void CheckSearchableUrls() {
  Check(IsSearchableUrl("https://example.com/a?b=1"), "a web page");
  Check(IsSearchableUrl("file:///C:/notes/a.html"), "a local page");
  Check(!IsSearchableUrl("edge://settings"), "an Edge page is Edge's");
  Check(!IsSearchableUrl("https://example.com/paper.PDF"), "a PDF is Edge's");
  Check(!IsSearchableUrl("https://example.com/paper.pdf#page=3"),
        "a PDF with a fragment");
  Check(IsSearchableUrl("https://example.com/page?file=paper.pdf"),
        "a query that names a PDF is still a page");
  Check(!IsSearchableUrl(""), "no address");
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

void CheckParseQuery() {
  Query query = ParseQuery("plain words");
  Check(query.mode == Mode::kText, "plain text is Edge's");
  query = ParseQuery("");
  Check(query.mode == Mode::kText, "an empty box is Edge's");
  query = ParseQuery("/\\d{3}-\\d{4}/");
  Check(query.mode == Mode::kRegExp && query.pattern == "\\d{3}-\\d{4}" &&
            query.flags.empty(),
        "a closed RegExp");
  query = ParseQuery("/price/i");
  Check(query.mode == Mode::kRegExp && query.pattern == "price" &&
            query.flags == "i",
        "a RegExp with a flag");
  query = ParseQuery("/a/b/gi");
  Check(query.mode == Mode::kRegExp && query.pattern == "a/b" &&
            query.flags == "gi",
        "a slash inside the pattern");
  Check(ParseQuery("/price").mode == Mode::kText,
        "a RegExp not closed yet is plain text");
  Check(ParseQuery("//").mode == Mode::kText, "an empty RegExp is plain text");
  Check(ParseQuery("/etc/passwd").mode == Mode::kText,
        "a path whose tail is not flags is plain text");
  Check(ParseQuery("/a/ii").mode == Mode::kText, "a repeated flag");
  Check(ParseQuery("/a/x").mode == Mode::kText, "an unknown flag");
  query = ParseQuery("? where is the pricing?");
  Check(query.mode == Mode::kAsk && query.pattern == "where is the pricing?",
        "a question");
  query = ParseQuery("?   spaced out  ");
  Check(query.mode == Mode::kAsk && query.pattern == "spaced out",
        "a question is trimmed");
  query = ParseQuery("? ");
  Check(query.mode == Mode::kAsk && query.pattern.empty(),
        "a question mark and a space is a question not yet typed");
  Check(ParseQuery("?id=3").mode == Mode::kText,
        "a question mark without a space is text");
  Check(ParseQuery("?").mode == Mode::kText, "a lone question mark");
}

void CheckBuildSearch() {
  Search search = BuildSearch(ParseQuery("/a.b/"));
  Check(search.problem == PatternProblem::kNone && search.source == "a.b" &&
            search.flags == "gm",
        "case sensitive unless it says i");
  search = BuildSearch(ParseQuery("/x/i"));
  Check(search.flags == "gmi", "i ignores case");
  search = BuildSearch(ParseQuery("/x/gmis"));
  Check(search.flags == "gmis", "g and m are always on, not twice");
  search = BuildSearch(ParseQuery("/(a/"));
  Check(search.problem == PatternProblem::kUnbalanced && search.source.empty(),
        "a bad pattern builds nothing");
  search = BuildSearch(ParseQuery("/(a+)+/"));
  Check(search.problem == PatternProblem::kNestedRepeat,
        "a pattern that could hang the page is refused");
  Check(std::string(kRegExpName) == "RegExp", "the mode is called RegExp");
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
}

void CheckSearchResults() {
  SearchResult result = ParseSearchResult(Fields({"ok", "12", "0", "3"}));
  Check(result.isOk && result.count == 12 && !result.isCapped &&
            result.current == 3,
        "ok");
  result = ParseSearchResult(Fields({"ok", "5000", "1", "0"}));
  Check(result.isOk && result.isCapped, "capped");
  result = ParseSearchResult(Fields({"err", "Invalid regular expression"}));
  Check(!result.isOk && result.error == "Invalid regular expression", "err");
  result = ParseSearchResult("garbage");
  Check(!result.isOk && !result.error.empty(), "unreadable answer");
  result = ParseSearchResult(Fields({"ok", "x", "0", "0"}));
  Check(!result.isOk, "a count that is not a number");
  result = ParseSearchResult("");
  Check(!result.isOk, "empty answer");
}

}  // namespace

int main() {
  try {
    CheckSearchableUrls();
    CheckPatternShape();
    CheckParseQuery();
    CheckBuildSearch();
    CheckStepping();
    CheckSearchResults();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
