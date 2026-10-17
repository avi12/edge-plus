// glic_text.h — the text the glic host and the DLL exchange. One concern, pure
// (no Edge, no Win32; unit-tested in test/glic_text_test.cpp): reading what the
// host's poll reports, and writing the JSON answers the host expects (tab data,
// the window's state, a tab's context).
#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "share_text.h"

namespace fixedge::glic_text {

// --- what the host's poll reports -----------------------------------------------

struct Call {
  std::string id;  // digits
  std::string operation;
  std::string json;  // the arguments
};

struct Report {
  bool isInstalled = false;    // the isolated-world script answered
  bool isBooted = false;       // the page took our patched library
  std::string client;          // "", "created", "initialized" or "failed"
  bool isUnavailable = false;  // the page says Gemini is not available
  std::vector<Call> calls;
};

inline constexpr char kPollMarker[] = "fixedge-glic";
inline constexpr char kCallLine[] = "call";
inline constexpr char kBootedPrefix[] = "booted=";
inline constexpr char kClientPrefix[] = "client=";
inline constexpr char kUnavailablePrefix[] = "unavailable=";
inline constexpr char kTab = '\t';

inline std::string ValueAfter(const std::string& line, const char* prefix) {
  return line.substr(std::string(prefix).size());
}

inline Report ParsePoll(const std::string& raw) {
  Report report;
  size_t start = 0;
  bool isFirstLine = true;
  while (start <= raw.size()) {
    size_t end = raw.find('\n', start);
    if (end == std::string::npos) {
      end = raw.size();
    }
    const std::string line = raw.substr(start, end - start);
    start = end + 1;
    if (isFirstLine) {
      isFirstLine = false;
      if (line != kPollMarker) {
        return report;
      }
      report.isInstalled = true;
      continue;
    }
    if (line.rfind(kBootedPrefix, 0) == 0) {
      report.isBooted = ValueAfter(line, kBootedPrefix) == "1";
    } else if (line.rfind(kClientPrefix, 0) == 0) {
      report.client = ValueAfter(line, kClientPrefix);
    } else if (line.rfind(kUnavailablePrefix, 0) == 0) {
      report.isUnavailable = ValueAfter(line, kUnavailablePrefix) == "1";
    } else if (line.rfind(kCallLine, 0) == 0) {
      // call TAB id TAB operation TAB json (the json is last: it may hold tabs)
      const size_t idStart = line.find(kTab);
      const size_t operationStart =
          idStart == std::string::npos ? idStart : line.find(kTab, idStart + 1);
      const size_t jsonStart = operationStart == std::string::npos
                                   ? operationStart
                                   : line.find(kTab, operationStart + 1);
      if (jsonStart == std::string::npos) {
        continue;
      }
      Call call;
      call.id = line.substr(idStart + 1, operationStart - idStart - 1);
      call.operation =
          line.substr(operationStart + 1, jsonStart - operationStart - 1);
      call.json = line.substr(jsonStart + 1);
      if (share_text::IsDigits(call.id)) {
        report.calls.push_back(call);
      }
    }
  }
  return report;
}

// The number after `"name":` in `json`, or `fallback` when it is absent or
// null (the host writes its arguments as flat JSON with plain names).
inline long long NumberField(const std::string& json, const char* name,
                             long long fallback) {
  const std::string key = std::string("\"") + name + "\":";
  const size_t keyAt = json.find(key);
  if (keyAt == std::string::npos) {
    return fallback;
  }
  size_t digitsStart = keyAt + key.size();
  const bool isNegative = digitsStart < json.size() && json[digitsStart] == '-';
  if (isNegative) {
    digitsStart++;
  }
  size_t digitsEnd = digitsStart;
  while (digitsEnd < json.size() && json[digitsEnd] >= '0' &&
         json[digitsEnd] <= '9') {
    digitsEnd++;
  }
  if (digitsEnd == digitsStart) {
    return fallback;
  }
  const long long value =
      std::stoll(json.substr(digitsStart, digitsEnd - digitsStart));
  return isNegative ? -value : value;
}

inline constexpr int kNoTabId = -1;

// The `context` request: which tab (kNoTabId = the focused one) and how many
// bytes of text (0 = no limit of the host's own).
struct ContextRequest {
  int tabId = kNoTabId;
  size_t maxBytes = 0;
};

inline ContextRequest ParseContextRequest(const std::string& json) {
  ContextRequest request;
  request.tabId = static_cast<int>(NumberField(json, "tabId", kNoTabId));
  const long long limit = NumberField(json, "innerTextBytesLimit", 0);
  request.maxBytes = limit > 0 ? static_cast<size_t>(limit) : 0;
  return request;
}

// Whether the page's glic UI is not coming: it says so, or the web client never
// even registered within `elapsedMs`. The caller then falls back to the plain
// Gemini web app.
inline bool ShouldFallBack(const Report& report, unsigned long long elapsedMs,
                           unsigned long long graceMs) {
  const bool isRefused = report.isBooted && report.isUnavailable;
  const bool isFailed = report.client == "failed";
  const bool isSilent = elapsedMs > graceMs && report.client.empty();
  return isRefused || isFailed || isSilent;
}

// --- the answers --------------------------------------------------------------------

// A tab as the host reads it.
struct HostTab {
  int tabId = 0;
  std::string url;
  std::string title;
  bool isActive = false;
  bool canShare = false;
};

inline constexpr int kWindowId = 1;

inline std::string TabJson(const HostTab& tab) {
  return "{\"tabId\":" + std::to_string(tab.tabId) +
         ",\"windowId\":" + std::to_string(kWindowId) +
         ",\"url\":" + share_text::JsStringLiteral(tab.url) +
         ",\"title\":" + share_text::JsStringLiteral(tab.title) +
         ",\"isActive\":" + (tab.isActive ? "true" : "false") +
         ",\"canShare\":" + (tab.canShare ? "true" : "false") + "}";
}

inline constexpr char kNoFocusReason[] =
    "The tab cannot be shared: it is not a web page.";
inline constexpr char kNoActiveTabReason[] = "There is no active tab.";

// The window's state: the focused tab (the active tab, if it can be shared),
// the active tab either way, and every tab.
inline std::string StateJson(const std::vector<HostTab>& tabs) {
  const HostTab* active = nullptr;
  for (const HostTab& tab : tabs) {
    if (tab.isActive) {
      active = &tab;
    }
  }
  const bool isFocused = active && active->canShare;
  std::string json =
      std::string("{\"focused\":") + (isFocused ? TabJson(*active) : "null") +
      ",\"active\":" + (active ? TabJson(*active) : "null") + ",\"reason\":" +
      share_text::JsStringLiteral(active ? kNoFocusReason
                                         : kNoActiveTabReason) +
      ",\"tabs\":[";
  for (size_t i = 0; i < tabs.size(); i++) {
    json += (i > 0 ? "," : "") + TabJson(tabs[i]);
  }
  return json + "]}";
}

// A tab's context for getContextFromTab: its data and its text, cut to
// `maxBytes` when that is set (the host's innerTextBytesLimit; 0 = no limit
// beyond the page script's own).
inline std::string ContextJson(const HostTab& tab,
                               const share_text::PageSnapshot& snapshot,
                               size_t maxBytes) {
  std::string text = snapshot.text;
  bool isTruncated = snapshot.isTruncated;
  if (maxBytes > 0 && text.size() > maxBytes) {
    text = share_text::TruncateUtf8(text, maxBytes);
    isTruncated = true;
  }
  return "{\"tab\":" + TabJson(tab) +
         ",\"innerText\":" + share_text::JsStringLiteral(text) +
         ",\"truncated\":" + (isTruncated ? "true" : "false") + "}";
}

inline std::string ErrorJson(const std::string& message) {
  return "{\"error\":" + share_text::JsStringLiteral(message) + "}";
}

}  // namespace fixedge::glic_text
