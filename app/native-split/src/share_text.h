// share_text.h — the text side of sharing a tab with Gemini. One concern, all of
// it pure (no Edge, no Win32; unit-tested in test/share_text_test.cpp): what a
// page hands back, the prompt block that goes into Gemini's box, the strings
// that cross into scripts, and the strip's poll report.
#pragma once
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace fixedge::share_text {

// How much of a page's text is shared: the page script cuts at this many
// characters, and the prompt says so when it did.
inline constexpr size_t kMaxPageChars = 30000;

// Fields of a page's answer, and of the strip's poll report lines.
inline constexpr char kFieldSeparator = '\x1f';
inline constexpr char kLineSeparator = '\n';

inline constexpr char kOkMarker[] = "ok";
inline constexpr char kErrorMarker[] = "err";

// --- what a page hands back ----------------------------------------------------

struct PageSnapshot {
  bool isOk = false;
  std::string error;  // why not, when !isOk
  std::string url;
  std::string title;
  bool isTruncated = false;
  std::string selection;  // what the user had selected on the page
  std::string text;
};

// The page script answers `ok US url US title US truncated(0|1) US selection US
// text` or `err US message` (US = kFieldSeparator). The text is last, so it may
// hold anything.
inline PageSnapshot ParseSnapshot(const std::string& raw) {
  PageSnapshot snapshot;
  std::vector<std::string> fields;
  size_t start = 0;
  constexpr size_t kOkFieldCount = 6;
  while (fields.size() + 1 < kOkFieldCount) {
    const size_t end = raw.find(kFieldSeparator, start);
    if (end == std::string::npos) {
      break;
    }
    fields.push_back(raw.substr(start, end - start));
    start = end + 1;
  }
  fields.push_back(raw.substr(start));
  const bool isError = fields[0] == kErrorMarker;
  if (isError) {
    snapshot.error = fields.size() > 1 ? fields[1] : std::string();
    return snapshot;
  }
  const bool isComplete =
      fields[0] == kOkMarker && fields.size() == kOkFieldCount;
  if (!isComplete) {
    snapshot.error = "The page gave an answer Edge could not read.";
    return snapshot;
  }
  snapshot.isOk = true;
  snapshot.url = fields[1];
  snapshot.title = fields[2];
  snapshot.isTruncated = fields[3] == "1";
  snapshot.selection = fields[4];
  snapshot.text = fields[5];
  return snapshot;
}

// Cut `text` to at most `maxBytes` without splitting a UTF-8 character.
inline std::string TruncateUtf8(const std::string& text, size_t maxBytes) {
  if (text.size() <= maxBytes) {
    return text;
  }
  size_t end = maxBytes;
  constexpr unsigned char kContinuationMask = 0xC0;
  constexpr unsigned char kContinuationBits = 0x80;
  while (end > 0 && (static_cast<unsigned char>(text[end]) &
                     kContinuationMask) == kContinuationBits) {
    end--;
  }
  return text.substr(0, end);
}

// --- which pages can be shared -------------------------------------------------

inline bool StartsWithIgnoreCase(const std::string& text, const char* prefix) {
  for (size_t i = 0; prefix[i] != '\0'; i++) {
    const bool isMatch =
        i < text.size() &&
        std::tolower(static_cast<unsigned char>(text[i])) == prefix[i];
    if (!isMatch) {
      return false;
    }
  }
  return true;
}

// Web pages and local files, as Chrome's own sharing allows; never an edge://
// page, an extension, a new tab or a blank page.
inline bool IsShareableUrl(const std::string& url) {
  return StartsWithIgnoreCase(url, "http://") ||
         StartsWithIgnoreCase(url, "https://") ||
         StartsWithIgnoreCase(url, "file://");
}

// The host of a web `url` ("github.com"), or the whole URL for any other kind
// (an edge:// page has no host worth showing).
inline std::string HostOf(const std::string& url) {
  const bool isWebUrl = StartsWithIgnoreCase(url, "http://") ||
                        StartsWithIgnoreCase(url, "https://");
  const size_t schemeEnd = url.find("://");
  if (!isWebUrl || schemeEnd == std::string::npos) {
    return url;
  }
  const size_t hostStart = schemeEnd + 3;
  const size_t hostEnd = url.find_first_of("/?#", hostStart);
  return url.substr(hostStart, hostEnd == std::string::npos
                                   ? std::string::npos
                                   : hostEnd - hostStart);
}

// --- the prompt block -----------------------------------------------------------

inline std::string FormatCount(size_t value) {
  std::string digits = std::to_string(value);
  for (size_t i = digits.size(); i > 3; i -= 3) {
    digits.insert(i - 3, ",");
  }
  return digits;
}

// What goes into Gemini's prompt box for one shared page. It is plain text the
// user reads and edits before sending; the cut, when there was one, is stated.
// It ends with a blank line so the user's own question follows.
inline std::string BuildPromptBlock(const PageSnapshot& snapshot) {
  std::string block = "Here is a web page I have open in Edge.\n";
  block += "Title: " + snapshot.title + "\n";
  block += "URL: " + snapshot.url + "\n";
  if (!snapshot.selection.empty()) {
    block += "\nThe part I have selected:\n" + snapshot.selection + "\n";
  }
  block += "\nPage text:\n---\n" + snapshot.text + "\n---\n";
  if (snapshot.isTruncated) {
    block += "[Shortened: only the first " + FormatCount(kMaxPageChars) +
             " characters of the page are included.]\n";
  }
  block += "\n";
  return block;
}

// --- crossing into a script or a URL -------------------------------------------

// `utf8` as a double-quoted JavaScript string literal.
inline std::string JsStringLiteral(const std::string& utf8) {
  std::string literal = "\"";
  for (const char character : utf8) {
    const unsigned char byte = static_cast<unsigned char>(character);
    switch (character) {
      case '"':
        literal += "\\\"";
        break;
      case '\\':
        literal += "\\\\";
        break;
      case '\n':
        literal += "\\n";
        break;
      case '\r':
        literal += "\\r";
        break;
      case '\t':
        literal += "\\t";
        break;
      default: {
        constexpr unsigned char kFirstPrintable = 0x20;
        if (byte < kFirstPrintable) {
          char escape[8];
          std::snprintf(escape, sizeof(escape), "\\u%04x", byte);
          literal += escape;
        } else {
          literal += character;
        }
      }
    }
  }
  literal += "\"";
  return literal;
}

// `html` as a data: URL, every byte that is not a plain URL character
// percent-encoded.
inline std::string BuildDataUrl(const std::string& html) {
  std::string url = "data:text/html;charset=utf-8,";
  for (const char character : html) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (std::isalnum(byte) != 0) {
      url += character;
    } else {
      char escape[4];
      std::snprintf(escape, sizeof(escape), "%%%02X", byte);
      url += escape;
    }
  }
  return url;
}

// --- a status line's kinds -------------------------------------------------------

// The kind of a status line in one of our pages (the share strip, the find bar),
// and the name the page's style keys on.
enum class StatusKind {
  kInfo,
  kOk,
  kError
};
inline const char* StatusKindName(StatusKind kind) {
  switch (kind) {
    case StatusKind::kInfo:
      return "info";
    case StatusKind::kOk:
      return "ok";
    case StatusKind::kError:
      return "error";
  }
  return "info";
}

// --- the tab list the picker shows ---------------------------------------------

enum class TabState {
  kReady,    // can be shared
  kAsleep,   // discarded to save memory: open it once, then share
  kBlocked,  // an Edge page, a new tab, an extension: never shared
};

inline const char* TabStateName(TabState state) {
  switch (state) {
    case TabState::kReady:
      return "ready";
    case TabState::kAsleep:
      return "asleep";
    case TabState::kBlocked:
      return "blocked";
  }
  return "blocked";
}

struct TabEntry {
  std::string id;  // opaque, digits only
  std::string title;
  std::string url;
  bool isCurrent = false;  // the tab the user is looking at
  TabState state = TabState::kBlocked;
};

// A JavaScript array literal of the picker's rows.
inline std::string BuildTabsLiteral(const std::vector<TabEntry>& tabs) {
  std::string literal = "[";
  for (size_t i = 0; i < tabs.size(); i++) {
    const TabEntry& tab = tabs[i];
    if (i > 0) {
      literal += ",";
    }
    literal += "{id:" + JsStringLiteral(tab.id) +
               ",title:" + JsStringLiteral(tab.title) +
               ",host:" + JsStringLiteral(HostOf(tab.url)) +
               ",current:" + (tab.isCurrent ? "true" : "false") +
               ",state:" + JsStringLiteral(TabStateName(tab.state)) + "}";
  }
  literal += "]";
  return literal;
}

// --- what the strip reports on each poll ---------------------------------------

enum class ActionKind {
  kShareActive,  // "Share this tab"
  kList,         // the picker opened: send the window's tabs
  kShareTabs,    // the picker's selection: share these, in order
};

struct Action {
  ActionKind kind = ActionKind::kShareActive;
  std::vector<std::string> tabIds;  // kShareTabs only; digits only
};

struct PollReport {
  bool isInstalled = false;  // the strip's script is running
  bool isExpanded = false;   // the picker is open: the strip covers the panel
  std::vector<Action> actions;
};

inline constexpr char kExpandedLine[] = "expanded=";
inline constexpr char kShareActiveLine[] = "share-active";
inline constexpr char kListLine[] = "list";
inline constexpr char kShareTabsPrefix[] = "share:";
inline constexpr char kInstalledMarker[] = "fixedge-strip";

inline bool IsDigits(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  for (const char character : text) {
    if (std::isdigit(static_cast<unsigned char>(character)) == 0) {
      return false;
    }
  }
  return true;
}

// The strip's drain answers `fixedge-strip`, then `expanded=0|1`, then one line
// per click. Anything else (the script is not there yet) is "not installed".
inline PollReport ParsePoll(const std::string& raw) {
  PollReport report;
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
      if (line != kInstalledMarker) {
        return report;
      }
      report.isInstalled = true;
      continue;
    }
    if (line.rfind(kExpandedLine, 0) == 0) {
      report.isExpanded = line.substr(sizeof(kExpandedLine) - 1) == "1";
    } else if (line == kShareActiveLine) {
      report.actions.push_back(Action{ActionKind::kShareActive, {}});
    } else if (line == kListLine) {
      report.actions.push_back(Action{ActionKind::kList, {}});
    } else if (line.rfind(kShareTabsPrefix, 0) == 0) {
      Action action{ActionKind::kShareTabs, {}};
      const std::string ids = line.substr(sizeof(kShareTabsPrefix) - 1);
      size_t idStart = 0;
      while (idStart <= ids.size()) {
        size_t idEnd = ids.find(',', idStart);
        if (idEnd == std::string::npos) {
          idEnd = ids.size();
        }
        const std::string tabId = ids.substr(idStart, idEnd - idStart);
        if (IsDigits(tabId)) {
          action.tabIds.push_back(tabId);
        }
        idStart = idEnd + 1;
      }
      report.actions.push_back(action);
    }
  }
  return report;
}

}  // namespace fixedge::share_text
