// Unit tests for the pure text side of tab sharing (share_text.h). No Edge/Win32
// deps. Run via test\run.cmd.
#include <cstdio>
#include <string>
#include "../src/share_text.h"

using namespace fixedge::share_text;

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

std::string Join(std::initializer_list<std::string> fields) {
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

void CheckSnapshots() {
  const PageSnapshot page = ParseSnapshot(
      Join({"ok", "https://a.test/x", "A title", "0", "", "Body"}));
  Check(page.isOk && page.url == "https://a.test/x" &&
            page.title == "A title" && !page.isTruncated &&
            page.selection.empty() && page.text == "Body",
        "an ok answer is read field by field");

  const PageSnapshot cut =
      ParseSnapshot(Join({"ok", "https://a.test", "T", "1", "picked", "Body"}));
  Check(cut.isOk && cut.isTruncated && cut.selection == "picked",
        "the truncated flag and the selection come through");

  std::string odd = Join({"ok", "https://a.test", "T", "0", "", "a"});
  odd += kFieldSeparator;
  odd += "b";
  Check(ParseSnapshot(odd).text == std::string("a") + kFieldSeparator + "b",
        "the text is last, so it may hold the separator");

  const PageSnapshot failed =
      ParseSnapshot(Join({"err", "PDF pages are not read"}));
  Check(!failed.isOk && failed.error == "PDF pages are not read",
        "an err answer carries its message");
  Check(!ParseSnapshot("garbage").isOk, "an unknown answer is not ok");
  Check(!ParseSnapshot("").isOk, "an empty answer is not ok");
}

void CheckTruncation() {
  Check(TruncateUtf8("hello", 10) == "hello", "a short text is untouched");
  Check(TruncateUtf8("hello", 3) == "hel", "a long text is cut");
  // "é" is two bytes (C3 A9): cutting between them must drop the whole letter.
  Check(TruncateUtf8("a\xC3\xA9z", 2) == "a", "a cut never splits a character");
  Check(TruncateUtf8("a\xC3\xA9z", 3) == "a\xC3\xA9",
        "a cut at a boundary keeps it");
}

void CheckUrls() {
  Check(IsShareableUrl("https://github.com/x"), "https is shareable");
  Check(IsShareableUrl("HTTP://example.com"), "the scheme is case-blind");
  Check(IsShareableUrl("file:///C:/notes.txt"), "a local file is shareable");
  Check(!IsShareableUrl("edge://settings"), "an edge page is not");
  Check(!IsShareableUrl("chrome-extension://abc/page.html"),
        "an extension is not");
  Check(!IsShareableUrl("about:blank"), "a blank page is not");
  Check(!IsShareableUrl(""), "no url is not");
  Check(HostOf("https://avi12.com/path?q=1") == "avi12.com",
        "host before path");
  Check(HostOf("https://avi12.com") == "avi12.com", "host with no path");
  Check(HostOf("about:blank") == "about:blank", "no host: the url itself");
}

void CheckPrompt() {
  PageSnapshot page;
  page.isOk = true;
  page.title = "Hello";
  page.url = "https://a.test/";
  page.text = "Body text";
  const std::string block = BuildPromptBlock(page);
  Check(block.find("Title: Hello") != std::string::npos &&
            block.find("URL: https://a.test/") != std::string::npos &&
            block.find("Body text") != std::string::npos,
        "the block carries title, url and text");
  Check(block.find("Shortened") == std::string::npos,
        "an uncut page says nothing about cutting");
  Check(block.find("selected") == std::string::npos,
        "no selection, no selection section");
  Check(block.size() >= 2 && block.substr(block.size() - 2) == "\n\n",
        "the block ends with a blank line for the question");

  page.isTruncated = true;
  page.selection = "chosen words";
  const std::string cut = BuildPromptBlock(page);
  Check(cut.find("Shortened: only the first 30,000 characters") !=
            std::string::npos,
        "a cut page says so, with the limit");
  Check(cut.find("chosen words") != std::string::npos,
        "the selection is included");
  Check(FormatCount(1234567) == "1,234,567" && FormatCount(999) == "999",
        "counts get thousands separators");
}

void CheckEscaping() {
  Check(JsStringLiteral("a\"b\\c\nd") == "\"a\\\"b\\\\c\\nd\"",
        "quotes, backslashes and newlines are escaped");
  Check(JsStringLiteral(std::string("x\x01y")) == "\"x\\u0001y\"",
        "control characters become \\u escapes");
  Check(JsStringLiteral("h\xC3\xA9") == "\"h\xC3\xA9\"",
        "UTF-8 passes through for the script to read");
  Check(BuildDataUrl("<b>a b</b>") ==
            "data:text/html;charset=utf-8,%3Cb%3Ea%20b%3C%2Fb%3E",
        "a data url percent-encodes everything but letters and digits");
}

void CheckTabsLiteral() {
  TabEntry tab;
  tab.id = "42";
  tab.title = "Say \"hi\"";
  tab.url = "https://avi12.com/x";
  tab.isCurrent = true;
  tab.state = TabState::kReady;
  Check(BuildTabsLiteral({tab}) ==
            "[{id:\"42\",title:\"Say \\\"hi\\\"\",host:\"avi12.com\","
            "current:true,state:\"ready\"}]",
        "a tab row is a JavaScript object literal");
  Check(BuildTabsLiteral({}) == "[]", "no tabs is an empty array");
}

void CheckPoll() {
  Check(!ParsePoll("").isInstalled, "an empty answer is not installed");
  Check(!ParsePoll("not-installed").isInstalled,
        "another first line is not it");

  const PollReport idle = ParsePoll("fixedge-strip\nexpanded=0");
  Check(idle.isInstalled && !idle.isExpanded && idle.actions.empty(),
        "an idle strip reports nothing");

  PollReport busy = ParsePoll(
      "fixedge-strip\nexpanded=1\nshare-active\nlist\nshare:12,34,x,56");
  Check(busy.isExpanded && busy.actions.size() == 3, "three clicks reported");
  Check(busy.actions[0].kind == ActionKind::kShareActive, "share-active first");
  Check(busy.actions[1].kind == ActionKind::kList, "list second");
  Check(busy.actions[2].kind == ActionKind::kShareTabs &&
            busy.actions[2].tabIds.size() == 3 &&
            busy.actions[2].tabIds[0] == "12" &&
            busy.actions[2].tabIds[2] == "56",
        "shared ids keep their order and non-digit ids are dropped");
}

}  // namespace

int main() {
  try {
    CheckSnapshots();
    CheckTruncation();
    CheckUrls();
    CheckPrompt();
    CheckEscaping();
    CheckTabsLiteral();
    CheckPoll();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
