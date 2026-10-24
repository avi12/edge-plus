// Unit tests for web_text.h.
#include <cstdio>
#include <string>
#include "../src/web_text.h"

using namespace edgeplus::web_text;

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

void CheckTruncation() {
  Check(TruncateUtf8("hello", 10) == "hello", "a short text is untouched");
  Check(TruncateUtf8("hello", 3) == "hel", "a long text is cut");
  // "é" is two bytes (C3 A9): cutting between them must drop the whole letter.
  Check(TruncateUtf8("a\xC3\xA9z", 2) == "a", "a cut never splits a character");
  Check(TruncateUtf8("a\xC3\xA9z", 3) == "a\xC3\xA9",
        "a cut at a boundary keeps it");
}

void CheckUrls() {
  Check(IsWebPageUrl("https://github.com/x"), "https is a web page");
  Check(IsWebPageUrl("HTTP://example.com"), "the scheme is case-blind");
  Check(IsWebPageUrl("file:///C:/notes.txt"), "a local file is a web page");
  Check(!IsWebPageUrl("edge://settings"), "an edge page is not");
  Check(!IsWebPageUrl("chrome-extension://abc/page.html"),
        "an extension is not");
  Check(!IsWebPageUrl("about:blank"), "a blank page is not");
  Check(!IsWebPageUrl(""), "no url is not");
}

void CheckEscaping() {
  Check(JsStringLiteral("a\"b\\c\nd") == "\"a\\\"b\\\\c\\nd\"",
        "quotes, backslashes and newlines are escaped");
  Check(JsStringLiteral(std::string("x\x01y")) == "\"x\\u0001y\"",
        "control characters become \\u escapes");
  Check(JsStringLiteral("h\xC3\xA9") == "\"h\xC3\xA9\"",
        "UTF-8 passes through for the script to read");
}

void CheckScriptAnswers() {
  Check(JsOkAnswer() == "\"ok\"", "a bare ok is the tag alone");
  Check(JsOkAnswer({"count", "\"1\""}) ==
            "[\"ok\", count, \"1\"].join(\"\\u001f\")",
        "an ok joins its fields with the separator");
  Check(JsErrorAnswer("No \"page\".") == "\"err\\u001fNo \\\"page\\\".\"",
        "a fixed error is one literal, escaped");
  Check(JsErrorAnswerFrom("error.message") == "\"err\\u001f\" + error.message",
        "a computed error is appended to the tag");
}

}  // namespace

int main() {
  try {
    CheckTruncation();
    CheckUrls();
    CheckEscaping();
    CheckScriptAnswers();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
