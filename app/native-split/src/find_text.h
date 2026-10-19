// find_text.h — the text side of the enhanced find. One concern, all of it pure
// (no Edge, no Win32; unit-tested in test/find_text_test.cpp): telling from what
// the user typed into Edge's find bar whether it is a RegExp or a question for
// the browser assistant, turning a RegExp into what the page script runs,
// checking it before it reaches a page, stepping through the matches, and
// reading what the page script reports back.
#pragma once
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>
#include "find_flags.h"
#include "web_text.h"

namespace fixedge::find_text {

// What Edge's find bar is asked for. Plain text is Edge's own search, untouched;
// the other two are ours.
enum class Mode {
  kText,    // the words typed: Edge's find
  kRegExp,  // /pattern/flags: a JavaScript regular expression
  kAsk,     // ? question: a question for the browser assistant (ask_text.h)
};

// The one name of the regular-expression mode in everything the user reads.
inline constexpr char kRegExpName[] = "RegExp";

// What a question is introduced by, and the delimiter of a RegExp.
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

inline bool IsSpace(char character) {
  return std::isspace(static_cast<unsigned char>(character)) != 0;
}

inline std::string TrimmedSpaces(const std::string& text) {
  size_t start = 0;
  size_t end = text.size();
  while (start < end && IsSpace(text[start])) {
    start++;
  }
  while (end > start && IsSpace(text[end - 1])) {
    end--;
  }
  return text.substr(start, end - start);
}

// Whether `flags` are RegExp flags we take, each at most once.
inline bool AreRegExpFlags(const std::string& flags) {
  for (size_t i = 0; i < flags.size(); i++) {
    const bool isKnown =
        std::string(kRegExpFlags).find(flags[i]) != std::string::npos;
    const bool isRepeated = flags.find(flags[i]) != i;
    if (!isKnown || isRepeated) {
      return false;
    }
  }
  return true;
}

// Read what is in the find bar's box. `? question` (a question mark and a space
// first) is a question; `/pattern/flags` (a slash, a pattern, the last slash,
// then only known flags) is a RegExp; anything else, including a RegExp not yet
// closed, is plain text for Edge.
inline Query ParseQuery(const std::string& typed) {
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
  const std::string flags = typed.substr(closing + 1);
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
inline std::string AskTitle(const std::string& assistantName) {
  return "Ask " + assistantName + " (Enter)";
}

inline bool StartsWith(const std::string& text, const std::string& prefix) {
  return text.compare(0, prefix.size(), prefix) == 0;
}

// Read the box with a switch on: what is typed is the pattern (case sensitive
// unless it starts with "(?i)") or the question. An empty box asks nothing.
inline Query ParseQuery(const std::string& typed, Option option) {
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
  const bool isIgnoringCase = StartsWith(typed, kIgnoreCasePrefix);
  query.pattern =
      isIgnoringCase ? typed.substr(sizeof(kIgnoreCasePrefix) - 1) : typed;
  query.flags = isIgnoringCase ? kIgnoreCaseFlag : "";
  return query;
}

// The box is a single line, so a line break in a RegExp is typed (Shift+Enter) or
// pasted as the two characters \n, which a RegExp reads as a line break exactly
// as a real one.
inline constexpr char kEscapedNewline[] = "\\n";

// Text pasted into the box with its line breaks (CR, LF or CRLF, each one break)
// written as \n.
inline std::string EscapeLineBreaks(const std::string& pasted) {
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

// Limits: a pattern longer than this is refused, and a page script stops
// marking after this many matches (it says so).
inline constexpr size_t kMaxPatternChars = 500;
inline constexpr int kMaxMatches = 5000;

// Fields of a report, and its lines (the same separators the page scripts use).
inline constexpr char kFieldSeparator = '\x1f';

// --- which pages the bar can search ------------------------------------------------

// Web pages and local files whose text is in the page. An Edge page (settings, a
// new tab) and a PDF (Edge's viewer draws it; its own find searches it) are left
// to Edge's stock find.
inline bool IsSearchableUrl(const std::string& url) {
  if (!web_text::IsWebPageUrl(url)) {
    return false;
  }
  const size_t pathEnd = url.find_first_of("?#");
  std::string path = url.substr(0, pathEnd);
  for (char& character : path) {
    character =
        static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  const std::string kPdfSuffix = ".pdf";
  const bool isPdf = path.size() >= kPdfSuffix.size() &&
                     path.compare(path.size() - kPdfSuffix.size(),
                                  kPdfSuffix.size(), kPdfSuffix) == 0;
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

inline const char* ProblemText(PatternProblem problem) {
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

inline bool IsRepeatChar(char character) {
  return character == '*' || character == '+';
}

inline bool StartsRepeat(char character) {
  return character == '*' || character == '+' || character == '{';
}

// Check a regular expression's shape before a page runs it. The page's own
// RegExp has the last word on syntax; this refuses what it would only report
// after the page had already started (a group left open is reported by the
// page, but a nested repeat such as (a+)+ is accepted by it and then runs for
// minutes), and keeps the message in one place.
inline PatternProblem CheckRegex(const std::string& pattern) {
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
inline std::string ScriptFlags(const std::string& typedFlags) {
  return find_flags::Merge(typedFlags + "gm", find_flags::FlagSet{});
}

// The search a RegExp query runs, or why it cannot.
inline Search BuildSearch(const Query& query) {
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

// The match to go to from `current` (kNoMatch before the first step) among
// `count`, wrapping at both ends.
inline int StepMatch(int current, int count, Step step) {
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

inline std::vector<std::string> SplitFields(const std::string& raw) {
  std::vector<std::string> fields;
  size_t start = 0;
  while (true) {
    const size_t end = raw.find(kFieldSeparator, start);
    if (end == std::string::npos) {
      fields.push_back(raw.substr(start));
      return fields;
    }
    fields.push_back(raw.substr(start, end - start));
    start = end + 1;
  }
}

inline bool IsDigits(const std::string& text) {
  if (text.empty() || text.size() > 9) {
    return false;
  }
  for (const char character : text) {
    if (character < '0' || character > '9') {
      return false;
    }
  }
  return true;
}

inline SearchResult ParseSearchResult(const std::string& raw) {
  SearchResult result;
  const std::vector<std::string> fields = SplitFields(raw);
  const bool isError = fields[0] == "err";
  if (isError) {
    result.error = fields.size() > 1 ? fields[1] : std::string();
    return result;
  }
  constexpr size_t kOkFieldCount = 4;
  const bool isComplete = fields[0] == "ok" && fields.size() == kOkFieldCount &&
                          IsDigits(fields[1]) && IsDigits(fields[3]);
  if (!isComplete) {
    result.error = "The page gave an answer Edge could not read.";
    return result;
  }
  result.isOk = true;
  result.count = std::stoi(fields[1]);
  result.isCapped = fields[2] == "1";
  result.current = std::stoi(fields[3]);
  return result;
}

}  // namespace fixedge::find_text
