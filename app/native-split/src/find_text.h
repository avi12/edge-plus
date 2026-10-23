// find_text.h — the text side of the enhanced find: parsing the find bar's box,
// checking a RegExp, stepping through matches and reading the page script's
// answer. Pure; tested in test/find_text_test.cpp.
#pragma once
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "find_flags.h"
#include "web_text.h"

namespace fixedge::find_text {

enum class Mode {
  kText,    // Edge's own find, untouched
  kRegExp,  // /pattern/flags
  kAsk,     // ? question, for the browser assistant (ask_text.h)
};

inline constexpr char kRegExpName[] = "RegExp";

inline constexpr char kAskMark = '?';
inline constexpr char kRegExpDelimiter = '/';
// The flags `/pattern/flags` takes: all eight of JavaScript's (find_flags.h says
// what each one does here). Typed, g and m are always on.
inline constexpr char kRegExpFlags[] = "dgimsuvy";

struct Query {
  Mode mode = Mode::kText;
  std::string pattern;  // kRegExp: the pattern; kAsk: the question, trimmed
  std::string flags;    // kRegExp: the flags typed after the closing slash
  // True when `flags` is already the whole story (the typed flags merged with
  // the bar's flag switches, which include g and m): nothing is implied.
  bool isFlagsFinal = false;
};

[[nodiscard]] inline bool IsSpace(char character) {
  return std::isspace(static_cast<unsigned char>(character)) != 0;
}

[[nodiscard]] inline std::string TrimmedSpaces(std::string_view text) {
  size_t start = 0;
  size_t end = text.size();
  while (start < end && IsSpace(text[start])) {
    start++;
  }
  while (end > start && IsSpace(text[end - 1])) {
    end--;
  }
  return std::string(text.substr(start, end - start));
}

// Each flag at most once.
[[nodiscard]] inline constexpr bool AreRegExpFlags(
    std::string_view flags) noexcept {
  for (size_t i = 0; i < flags.size(); i++) {
    const bool isKnown =
        std::string_view(kRegExpFlags).find(flags[i]) != std::string_view::npos;
    const bool isRepeated = flags.find(flags[i]) != i;
    if (!isKnown || isRepeated) {
      return false;
    }
  }
  return true;
}

// Anything not a complete `? question` or `/pattern/flags`, including a RegExp
// not yet closed, is plain text for Edge.
[[nodiscard]] inline Query ParseQuery(std::string_view typed) {
  Query query;
  const bool isQuestion =
      typed.size() >= 2 && typed[0] == kAskMark && IsSpace(typed[1]);
  if (isQuestion) {
    query.mode = Mode::kAsk;
    query.pattern = TrimmedSpaces(typed.substr(1));
    return query;
  }
  const size_t closing = typed.rfind(kRegExpDelimiter);
  const bool isDelimited =
      typed.size() >= 3 && typed[0] == kRegExpDelimiter && closing > 1;
  if (!isDelimited) {
    return query;
  }
  const std::string_view flags = typed.substr(closing + 1);
  if (!AreRegExpFlags(flags)) {
    return query;
  }
  query.mode = Mode::kRegExp;
  query.pattern = typed.substr(1, closing - 1);
  query.flags = flags;
  return query;
}

// Our two switches in the bar's options (its "..." button). With one on, the
// whole box is the pattern or the question and the typed forms above are not
// read; with none on they still work as shortcuts.
enum class Option {
  kNone,
  kRegExp,
  kAsk,
};

// A RegExp from the switch is case sensitive (JavaScript's own default), unless
// it starts with this: it is dropped and the search ignores case.
inline constexpr char kIgnoreCasePrefix[] = "(?i)";
inline constexpr char kIgnoreCaseFlag[] = "i";

// The switch for a question, named for the assistant that will answer it. It
// also says how to send: the box itself stays free for the question.
[[nodiscard]] inline std::string AskTitle(std::string_view assistantName) {
  std::string title = "Ask ";
  title += assistantName;
  return title + " (Enter)";
}

[[nodiscard]] inline Query ParseQuery(std::string_view typed, Option option) {
  if (option == Option::kNone) {
    return ParseQuery(typed);
  }
  Query query;
  const std::string question = TrimmedSpaces(typed);
  if (question.empty()) {
    return query;
  }
  if (option == Option::kAsk) {
    query.mode = Mode::kAsk;
    query.pattern = question;
    return query;
  }
  query.mode = Mode::kRegExp;
  const bool isIgnoringCase = typed.starts_with(kIgnoreCasePrefix);
  query.pattern =
      isIgnoringCase ? typed.substr(sizeof(kIgnoreCasePrefix) - 1) : typed;
  query.flags = isIgnoringCase ? kIgnoreCaseFlag : "";
  return query;
}

// Shown around the box's text while the RegExp switch is on, never part of it:
// the slashes of a JavaScript literal and the flags the search really runs with.
struct LiteralDecoration {
  std::string prefix;
  std::string suffix;
};

[[nodiscard]] inline LiteralDecoration RegExpDecoration(
    std::string_view typed, const find_flags::FlagSet& switches) {
  const Query query = ParseQuery(typed, Option::kRegExp);
  const std::string slash(1, kRegExpDelimiter);
  return LiteralDecoration{slash,
                           slash + find_flags::Merge(query.flags, switches)};
}

// The box is a single line, so a line break in a RegExp is typed (Shift+Enter) or
// pasted as the two characters \n, which a RegExp reads as a line break exactly
// as a real one.
inline constexpr char kEscapedNewline[] = "\\n";

[[nodiscard]] inline std::string EscapeLineBreaks(std::string_view pasted) {
  std::string escaped;
  for (size_t i = 0; i < pasted.size(); i++) {
    const char character = pasted[i];
    const bool isCarriageReturn = character == '\r';
    const bool isCrLf =
        isCarriageReturn && i + 1 < pasted.size() && pasted[i + 1] == '\n';
    if (isCrLf) {
      continue;
    }
    const bool isLineBreak = isCarriageReturn || character == '\n';
    if (isLineBreak) {
      escaped += kEscapedNewline;
    } else {
      escaped.push_back(character);
    }
  }
  return escaped;
}

inline constexpr size_t kMaxPatternChars = 500;
inline constexpr int kMaxMatches = 5000;

// --- which pages the bar can search ------------------------------------------------

// A PDF is drawn by Edge's viewer, whose own find searches it.
[[nodiscard]] inline bool IsSearchableUrl(std::string_view url) {
  if (!web_text::IsWebPageUrl(url)) {
    return false;
  }
  const size_t pathEnd = url.find_first_of("?#");
  std::string path(url.substr(0, pathEnd));
  for (char& character : path) {
    character =
        static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  constexpr std::string_view kPdfSuffix = ".pdf";
  const bool isPdf = path.ends_with(kPdfSuffix);
  return !isPdf;
}

// --- the regular expression ----------------------------------------------------

enum class PatternProblem {
  kNone,
  kEmpty,  // nothing typed: not an error, just nothing to search
  kTooLong,
  kUnbalanced,  // a group or a character class that never closes (or closes twice)
  kDanglingEscape,  // a lone backslash at the end
  kNestedRepeat,    // a repeated group that repeats inside: can hang the page
};

[[nodiscard]] inline constexpr const char* ProblemText(
    PatternProblem problem) noexcept {
  switch (problem) {
    case PatternProblem::kNone:
    case PatternProblem::kEmpty:
      return "";
    case PatternProblem::kTooLong:
      return "That pattern is too long.";
    case PatternProblem::kUnbalanced:
      return "A bracket or parenthesis is not closed.";
    case PatternProblem::kDanglingEscape:
      return "The pattern ends with a lone backslash.";
    case PatternProblem::kNestedRepeat:
      return "That pattern repeats a repeat and could hang the page.";
  }
  return "";
}

[[nodiscard]] inline constexpr bool IsRepeatChar(char character) noexcept {
  return character == '*' || character == '+';
}

[[nodiscard]] inline constexpr bool StartsRepeat(char character) noexcept {
  return character == '*' || character == '+' || character == '{';
}

// Check a regular expression's shape before a page runs it. The page's own
// RegExp has the last word on syntax; this refuses what it would only report
// after the page had already started (a group left open is reported by the
// page, but a nested repeat such as (a+)+ is accepted by it and then runs for
// minutes), and keeps the message in one place.
[[nodiscard]] inline PatternProblem CheckRegex(std::string_view pattern) {
  if (pattern.empty()) {
    return PatternProblem::kEmpty;
  }
  if (pattern.size() > kMaxPatternChars) {
    return PatternProblem::kTooLong;
  }
  // For each open group: whether its body ended in a repeat.
  std::vector<bool> groupEndsInRepeat;
  bool isInClass = false;
  bool isPreviousRepeat = false;
  for (size_t i = 0; i < pattern.size(); i++) {
    const char character = pattern[i];
    if (character == '\\') {
      const bool isLast = i + 1 == pattern.size();
      if (isLast) {
        return PatternProblem::kDanglingEscape;
      }
      i++;
      isPreviousRepeat = false;
    } else if (isInClass) {
      isInClass = character != ']';
      isPreviousRepeat = false;
    } else if (character == '[') {
      isInClass = true;
      isPreviousRepeat = false;
    } else if (character == '(') {
      groupEndsInRepeat.push_back(false);
      isPreviousRepeat = false;
    } else if (character == ')') {
      if (groupEndsInRepeat.empty()) {
        return PatternProblem::kUnbalanced;
      }
      const bool isBodyRepeating = groupEndsInRepeat.back();
      groupEndsInRepeat.pop_back();
      const bool isRepeatedNext =
          i + 1 < pattern.size() && StartsRepeat(pattern[i + 1]);
      if (isBodyRepeating && isRepeatedNext) {
        return PatternProblem::kNestedRepeat;
      }
      isPreviousRepeat = false;
    } else {
      // A "?" right after a repeat only makes it lazy: it is still a repeat.
      const bool isLazyMark = character == '?' && isPreviousRepeat;
      isPreviousRepeat = IsRepeatChar(character) || isLazyMark;
    }
    if (!groupEndsInRepeat.empty()) {
      groupEndsInRepeat.back() = isPreviousRepeat;
    }
  }
  const bool isOpen = isInClass || !groupEndsInRepeat.empty();
  return isOpen ? PatternProblem::kUnbalanced : PatternProblem::kNone;
}

// What the page script builds its RegExp from.
struct Search {
  PatternProblem problem = PatternProblem::kEmpty;
  std::string source;
  std::string flags;
};

// The flags the page script runs for a typed `/pattern/flags`: "g" finds every
// match and "m" lets ^ and $ stand for the start and end of a line (a page's
// text is one line per block), and the typed ones add the rest (a RegExp is case
// sensitive unless it says "i", as in JavaScript). In JavaScript's canonical
// order.
[[nodiscard]] inline std::string ScriptFlags(std::string_view typedFlags) {
  return find_flags::Merge(std::string(typedFlags) + "gm",
                           find_flags::FlagSet{});
}

[[nodiscard]] inline Search BuildSearch(const Query& query) {
  Search search;
  search.problem = CheckRegex(query.pattern);
  if (search.problem != PatternProblem::kNone) {
    return search;
  }
  search.source = query.pattern;
  search.flags = query.isFlagsFinal ? query.flags : ScriptFlags(query.flags);
  return search;
}

// --- stepping through matches ----------------------------------------------------

enum class Step {
  kNext,
  kPrevious,
};

inline constexpr int kNoMatch = -1;

// Wraps at both ends; `current` is kNoMatch before the first step.
[[nodiscard]] inline constexpr int StepMatch(int current, int count,
                                             Step step) noexcept {
  if (count <= 0) {
    return kNoMatch;
  }
  if (current < 0 || current >= count) {
    return step == Step::kNext ? 0 : count - 1;
  }
  if (step == Step::kNext) {
    return current + 1 == count ? 0 : current + 1;
  }
  return current == 0 ? count - 1 : current - 1;
}

// --- what the page script answers ---------------------------------------------------

// A search or a highlight answers `ok US count US isCapped(0|1) US current` or
// `err US message`. `current` is the match the page scrolled to.
struct SearchResult {
  bool isOk = false;
  int count = 0;
  bool isCapped = false;  // the page held more than kMaxMatches
  int current = 0;
  std::string error;
};

// Any more could overflow an int.
inline constexpr size_t kMaxCountDigits = 9;

[[nodiscard]] inline constexpr bool IsDigits(std::string_view text) noexcept {
  if (text.empty() || text.size() > kMaxCountDigits) {
    return false;
  }
  for (const char character : text) {
    if (character < '0' || character > '9') {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline SearchResult ParseSearchResult(std::string_view raw) {
  SearchResult result;
  const std::vector<std::string> fields = web_text::SplitFields(raw);
  const bool isError = fields[0] == web_text::kErrorTag;
  if (isError) {
    result.error = fields.size() > 1 ? fields[1] : std::string();
    return result;
  }
  constexpr size_t kOkFieldCount = 4;
  const bool isComplete = fields[0] == web_text::kOkTag &&
                          fields.size() == kOkFieldCount &&
                          IsDigits(fields[1]) && IsDigits(fields[3]);
  if (!isComplete) {
    result.error = web_text::kUnreadableAnswer;
    return result;
  }
  result.isOk = true;
  result.count = std::stoi(fields[1]);
  result.isCapped = web_text::IsTrueField(fields[2]);
  result.current = std::stoi(fields[3]);
  return result;
}

}  // namespace fixedge::find_text
