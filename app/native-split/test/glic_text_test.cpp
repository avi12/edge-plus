// Unit tests for the glic host's text (glic_text.h). No Edge/Win32 deps. Run via
// test\run.cmd.
#include <cstdio>
#include <string>
#include "../src/glic_text.h"

using namespace fixedge::glic_text;
using fixedge::share_text::PageSnapshot;

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

void CheckPoll() {
  Check(!ParsePoll("").isInstalled, "an empty poll is not installed");
  Check(!ParsePoll("not-installed").isInstalled,
        "another first line is not it");
  const Report idle =
      ParsePoll("fixedge-glic\nbooted=\nclient=\nunavailable=0");
  Check(idle.isInstalled && !idle.isBooted && idle.client.empty() &&
            !idle.isUnavailable && idle.calls.empty(),
        "an idle host reports its markers only");
  const Report busy = ParsePoll(
      "fixedge-glic\nbooted=1\nclient=initialized\nunavailable=1\n"
      "call\t7\tcontext\t{\"tabId\":null}\ncall\tx\tstate\tnull\n"
      "call\t8\tstate\tnull");
  Check(busy.isBooted && busy.client == "initialized" && busy.isUnavailable,
        "markers are read");
  Check(busy.calls.size() == 2 && busy.calls[0].id == "7" &&
            busy.calls[0].operation == "context" &&
            busy.calls[0].json == "{\"tabId\":null}" &&
            busy.calls[1].operation == "state",
        "calls are read in order and a call with a bad id is dropped");
}

void CheckRequests() {
  const ContextRequest focused = ParseContextRequest(
      "{\"tabId\":null,\"innerTextBytesLimit\":0,\"wantText\":true}");
  Check(focused.tabId == kNoTabId && focused.maxBytes == 0,
        "a null tab is the focused tab");
  const ContextRequest named = ParseContextRequest(
      "{\"tabId\":12,\"innerTextBytesLimit\":2048,\"wantText\":true}");
  Check(named.tabId == 12 && named.maxBytes == 2048, "a named tab and a limit");
  Check(NumberField("{\"tabId\":7}", "tabId", 0) == 7 &&
            NumberField("{}", "tabId", 3) == 3 &&
            NumberField("{\"tabId\":-5}", "tabId", 0) == -5,
        "numbers, absent fields and negatives");
}

void CheckFallback() {
  Report report;
  Check(!ShouldFallBack(report, 1000, 15000), "waiting is not failing");
  Check(ShouldFallBack(report, 16000, 15000),
        "a web client that never registered, after the grace, falls back");
  report.client = "initialized";
  Check(!ShouldFallBack(report, 16000, 15000), "a running client stays");
  report.isBooted = true;
  report.isUnavailable = true;
  Check(ShouldFallBack(report, 1000, 15000),
        "a page that says it is unavailable falls back at once");
  report.isUnavailable = false;
  report.client = "failed";
  Check(ShouldFallBack(report, 1000, 15000), "a failed initialize falls back");
}

void CheckAnswers() {
  HostTab active;
  active.tabId = 4;
  active.url = "https://a.test/";
  active.title = "A \"title\"";
  active.isActive = true;
  active.canShare = true;
  Check(
      TabJson(active) ==
          "{\"tabId\":4,\"windowId\":1,\"url\":\"https://a.test/\","
          "\"title\":\"A \\\"title\\\"\",\"isActive\":true,\"canShare\":true}",
      "a tab is a JSON object");

  HostTab other;
  other.tabId = 5;
  other.url = "edge://settings/";
  other.title = "Settings";
  const std::string state = StateJson({active, other});
  Check(state.find("\"focused\":{\"tabId\":4") != std::string::npos &&
            state.find("\"tabs\":[{\"tabId\":4") != std::string::npos &&
            state.find("{\"tabId\":5") != std::string::npos,
        "an active shareable tab is the focused one");

  active.canShare = false;
  const std::string blocked = StateJson({active, other});
  Check(blocked.find("\"focused\":null") != std::string::npos &&
            blocked.find("\"active\":{\"tabId\":4") != std::string::npos,
        "an active tab that cannot be shared is active but not focused");
  Check(StateJson({}).find("\"active\":null") != std::string::npos,
        "no tabs, no active tab");

  PageSnapshot snapshot;
  snapshot.text = "abcdefghij";
  const std::string whole = ContextJson(active, snapshot, 0);
  Check(whole.find("\"innerText\":\"abcdefghij\"") != std::string::npos &&
            whole.find("\"truncated\":false") != std::string::npos,
        "no limit keeps the whole text");
  const std::string cut = ContextJson(active, snapshot, 4);
  Check(cut.find("\"innerText\":\"abcd\"") != std::string::npos &&
            cut.find("\"truncated\":true") != std::string::npos,
        "a limit cuts the text and says so");
  Check(ErrorJson("no") == "{\"error\":\"no\"}", "an error is a JSON object");
}

}  // namespace

int main() {
  try {
    CheckPoll();
    CheckRequests();
    CheckFallback();
    CheckAnswers();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
