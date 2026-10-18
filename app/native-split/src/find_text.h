// find_text.h — the text side of the enhanced find bar. One concern, all of it
// pure (no Edge, no Win32; unit-tested in test/find_text_test.cpp): turning what
// the user typed into the regular expression the page script runs, checking that
// expression before it reaches a page, stepping through the matches, and reading
// what the bar page and the page script report back.
#pragma once
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>
#include "share_text.h"

namespace fixedge::find_text {

// How the bar reads its input box.
enum class Mode {
  kText,   // the words typed, literally
  kRegex,  // a JavaScript regular expression
  kAsk,    // a question for Copilot (ask_text.h)
};

struct Options {
  Mode mode = Mode::kText;
  bool isCaseSensitive = false;
  bool isWholeWord = false;
};

// Limits: a pattern longer than this is refused, and a page script stops
// marking after this many matches (it says so).
inline constexpr size_t kMaxPatternChars = 500;
inline constexpr int kMaxMatches = 5000;

// Fields of a report, and its lines (the same separators share_text.h uses).
inline constexpr char kFieldSeparator = '\x1f';
inline constexpr char kLineSeparator = '\n';

// --- which pages the bar can search ------------------------------------------------

// Web pages and local files whose text is in the page. An Edge page (settings, a
// new tab) and a PDF (Edge's viewer draws it; its own find searches it) are left
// to Edge's stock find.
inline bool IsSearchableUrl(const std::string& url) {
  if (!share_text::IsShareableUrl(url)) {
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

// Escape every character a regular expression gives a meaning to.
inline std::string EscapeLiteral(const std::string& text) {
  static const std::string kSpecial = "\\^$.*+?()[]{}|/-";
  std::string escaped;
  for (const char character : text) {
    if (kSpecial.find(character) != std::string::npos) {
      escaped.push_back('\\');
    }
    escaped.push_back(character);
  }
  return escaped;
}

// What the page script builds its RegExp from.
struct Search {
  PatternProblem problem = PatternProblem::kEmpty;
  std::string source;
  std::string flags;
};

// "g" finds every match, "m" lets ^ and $ stand for the start and end of a
// line (a page's text is one line per block), "i" ignores case.
inline Search BuildSearch(const std::string& typed, const Options& options) {
  Search search;
  const bool isRegex = options.mode == Mode::kRegex;
  search.problem = isRegex ? CheckRegex(typed) : PatternProblem::kNone;
  if (!isRegex) {
    if (typed.empty()) {
      search.problem = PatternProblem::kEmpty;
    } else if (typed.size() > kMaxPatternChars) {
      search.problem = PatternProblem::kTooLong;
    }
  }
  if (search.problem != PatternProblem::kNone) {
    return search;
  }
  search.source = isRegex ? typed : EscapeLiteral(typed);
  if (options.isWholeWord) {
    // Look-arounds, not \b: a word edge is only a boundary next to a word
    // character, and the typed text may start or end with a symbol.
    search.source = "(?<!\\w)(?:" + search.source + ")(?!\\w)";
  }
  search.flags = options.isCaseSensitive ? "gm" : "gim";
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

// "3 of 12", or why there is nothing to count.
inline std::string MatchLabel(int current, int count, bool isCapped) {
  if (count <= 0) {
    return "No results";
  }
  const std::string total =
      std::to_string(count) + (isCapped ? "+" : std::string());
  if (current < 0) {
    return total + (count == 1 && !isCapped ? " result" : " results");
  }
  return std::to_string(current + 1) + " of " + total;
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

// --- what the bar page reports -------------------------------------------------------

enum class EventKind {
  kQuery,  // the box, a mode or an option changed: search again
  kNext,
  kPrevious,
  kAsk,  // Enter in the question box
  kClose,
};

struct Event {
  EventKind kind = EventKind::kClose;
  Options options;
  std::string text;  // the box's contents, for kQuery and kAsk
};

struct Report {
  bool isInstalled = false;
  std::vector<Event> events;
};

inline constexpr char kBarMarker[] = "fixedge-find";
inline constexpr char kNotInstalledAnswer[] = "not-installed";

inline bool ModeFromName(const std::string& name, Mode* mode) {
  if (name == "text") {
    *mode = Mode::kText;
  } else if (name == "regex") {
    *mode = Mode::kRegex;
  } else if (name == "ask") {
    *mode = Mode::kAsk;
  } else {
    return false;
  }
  return true;
}

// The bar's poll answer: `fixedge-find` on the first line, then one event per
// line: `next`, `prev`, `close`, or `query|ask US mode US case(0|1) US
// word(0|1) US text`. A line it cannot read is dropped.
inline Report ParseReport(const std::string& raw) {
  Report report;
  size_t start = 0;
  bool isFirstLine = true;
  while (start <= raw.size()) {
    size_t end = raw.find(kLineSeparator, start);
    if (end == std::string::npos) {
      end = raw.size();
    }
    const std::string line = raw.substr(start, end - start);
    start = end + 1;
    if (isFirstLine) {
      isFirstLine = false;
      report.isInstalled = line == kBarMarker;
      if (!report.isInstalled) {
        return report;
      }
      continue;
    }
    Event event;
    if (line == "next") {
      event.kind = EventKind::kNext;
    } else if (line == "prev") {
      event.kind = EventKind::kPrevious;
    } else if (line == "close") {
      event.kind = EventKind::kClose;
    } else {
      const std::vector<std::string> fields = SplitFields(line);
      constexpr size_t kBoxFieldCount = 5;
      const bool isQuery = fields[0] == "query";
      const bool isAsk = fields[0] == "ask";
      const bool isBoxEvent =
          (isQuery || isAsk) && fields.size() == kBoxFieldCount;
      if (!isBoxEvent || !ModeFromName(fields[1], &event.options.mode)) {
        continue;
      }
      event.kind = isQuery ? EventKind::kQuery : EventKind::kAsk;
      event.options.isCaseSensitive = fields[2] == "1";
      event.options.isWholeWord = fields[3] == "1";
      event.text = fields[4];
    }
    report.events.push_back(event);
  }
  return report;
}

}  // namespace fixedge::find_text
