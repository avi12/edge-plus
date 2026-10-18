// Unit tests for the pure text side of "Ask Copilot" (ask_text.h). No Edge/Win32
// deps. Run via test\run.cmd.
#include <cstdio>
#include <string>
#include <vector>
#include "../src/ask_text.h"

using namespace fixedge::ask_text;

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
      joined += fixedge::find_text::kFieldSeparator;
    }
    isFirst = false;
    joined += field;
  }
  return joined;
}

std::string Joined(const std::vector<std::string>& paragraphs) {
  std::string joined;
  for (size_t i = 0; i < paragraphs.size(); i++) {
    joined +=
        (i == 0 ? "" : std::string(1, kParagraphSeparator)) + paragraphs[i];
  }
  return joined;
}

void CheckPageParagraphs() {
  PageParagraphs page = ParsePageParagraphs(
      Fields({"ok", "1", Joined({"First paragraph here.", "Second one."})}));
  Check(page.isOk && page.isCut && page.texts.size() == 2 &&
            page.texts[1] == "Second one.",
        "two paragraphs");
  page = ParsePageParagraphs(Fields({"ok", "0", ""}));
  Check(page.isOk && !page.isCut && page.texts.empty(), "an empty page");
  page = ParsePageParagraphs(Fields({"err", "PDF pages cannot be read yet."}));
  Check(!page.isOk && page.error == "PDF pages cannot be read yet.", "err");
  page = ParsePageParagraphs("nonsense");
  Check(!page.isOk && !page.error.empty(), "unreadable");
}

void CheckPrepare() {
  const std::vector<Paragraph> kept =
      Prepare({"Home", "Pricing starts at ten dollars\na month.", "",
               std::string(2000, 'x')});
  Check(kept.size() == 2, "short entries are dropped");
  Check(kept[0].index == 1 &&
            kept[0].text == "Pricing starts at ten dollars a month.",
        "the index is the page's, whitespace is flattened");
  Check(kept[1].index == 3 && kept[1].text.size() == kMaxParagraphChars + 3 &&
            kept[1].text.substr(kMaxParagraphChars) == "...",
        "a long paragraph is cut");
}

void CheckPlan() {
  std::vector<Paragraph> paragraphs;
  paragraphs.push_back(Paragraph{0, "Alpha paragraph about nothing."});
  paragraphs.push_back(Paragraph{4, "Beta paragraph about pricing."});
  Plan plan = PlanQuestion("where do they discuss pricing?", paragraphs);
  Check(plan.chunks.size() == 1 && !plan.isCut, "a short page is one prompt");
  const Chunk& chunk = plan.chunks[0];
  Check(chunk.indices.size() == 2 && chunk.indices[1] == 4, "its indices");
  Check(chunk.prompt.find("Question: \"where do they discuss pricing?\"") !=
            std::string::npos,
        "the prompt asks the question");
  Check(chunk.prompt.find("[P0] Alpha paragraph") != std::string::npos &&
            chunk.prompt.find("[P4] Beta paragraph") != std::string::npos,
        "paragraphs carry the page's labels");
  Check(chunk.prompt.find(kNoneWord) != std::string::npos,
        "the prompt allows NONE");

  paragraphs.clear();
  for (int i = 0; i < 100; i++) {
    paragraphs.push_back(Paragraph{i, std::string(kMaxParagraphChars, 'a')});
  }
  plan = PlanQuestion("q", paragraphs);
  Check(plan.chunks.size() == kMaxChunks && plan.isCut,
        "a long page is cut at the cap");
  for (const Chunk& each : plan.chunks) {
    Check(each.prompt.size() < kChunkChars + 2000,
          "each prompt stays inside its budget");
  }
  Check(plan.chunks[1].indices[0] > plan.chunks[0].indices.back(),
        "chunks continue where the last stopped");
  plan = PlanQuestion("q", {});
  Check(plan.chunks.empty() && !plan.isCut, "no paragraphs, no prompts");
  plan = PlanQuestion(std::string(kMaxQuestionChars * 3, 'q'),
                      {Paragraph{1, "some text here ok"}});
  Check(plan.chunks[0].prompt.find(std::string(kMaxQuestionChars + 1, 'q')) ==
            std::string::npos,
        "a long question is cut");
}

void CheckReply() {
  Reply reply = ParseReply(Fields({"ok", "P3, P7. Pricing is on both."}));
  Check(reply.isOk && reply.text == "P3, P7. Pricing is on both.", "ok");
  reply = ParseReply(Fields({"err", "It did not answer."}));
  Check(!reply.isOk && reply.error == "It did not answer.", "err");
  reply = ParseReply("junk");
  Check(!reply.isOk && !reply.error.empty(), "junk");
}

void CheckAnswer() {
  const std::vector<int> offered = {2, 3, 7, 12};
  Answer answer =
      ParseAnswer("P3, P7\nThe pricing table and the FAQ.", offered);
  Check(answer.indices.size() == 2 && answer.indices[0] == 3 &&
            answer.indices[1] == 7,
        "labels in order");
  Check(answer.note == "The pricing table and the FAQ.",
        "the sentence after the labels");
  answer = ParseAnswer("[P12] and [P2]", offered);
  Check(answer.indices.size() == 2 && answer.indices[0] == 12,
        "bracketed labels");
  answer = ParseAnswer("p3 p3 P3", offered);
  Check(answer.indices.size() == 1, "no repeats, either case");
  answer = ParseAnswer("P99, P3, P1", offered);
  Check(answer.indices.size() == 1 && answer.indices[0] == 3,
        "labels that were not offered are ignored");
  answer = ParseAnswer("NONE", offered);
  Check(answer.isNone && answer.indices.empty(), "none");
  answer = ParseAnswer("none - nothing mentions it.", offered);
  Check(answer.isNone, "none, lowercase");
  answer = ParseAnswer("None of them, sorry, but see P3.", offered);
  Check(!answer.isNone && answer.indices.size() == 1,
        "a label wins over a none");
  answer = ParseAnswer("I could not find a match.", offered);
  Check(!answer.isNone && answer.indices.empty(),
        "an answer with nothing usable");
  answer = ParseAnswer("PRICE12 is not a label, nor is P3x or Pp3", offered);
  Check(answer.indices.empty(), "only whole labels count");
  answer = ParseAnswer("P2 Pricing is covered here.", offered);
  Check(answer.indices.size() == 1 && answer.note == "Pricing is covered here.",
        "a word starting with P after a label is kept in the note");
  answer = ParseAnswer("P2, P3, P7, P12, P2", {2, 3, 7, 12, 13, 14});
  Check(answer.indices.size() == 4, "up to the per-reply cap");
  answer = ParseAnswer("P3, P7 Pricing is in both. Message Copilot Explore",
                       offered);
  Check(answer.indices.size() == 2 && answer.note == "Pricing is in both.",
        "the note stops at the end of the first sentence");
  answer = ParseAnswer("P3\nFirst line.\nSecond line.", offered);
  Check(answer.note == "First line.", "or at the end of the line");
  answer = ParseAnswer("P3 " + std::string(500, 'x'), offered);
  Check(answer.note.size() == kMaxNoteChars + 3,
        "a sentence that never ends is cut");
}

void CheckAccumulate() {
  std::vector<int> highlights;
  Answer first;
  first.indices = {5, 1};
  Answer second;
  second.indices = {1, 9};
  Accumulate(&highlights, first);
  Accumulate(&highlights, second);
  Check(highlights.size() == 3 && highlights[2] == 9, "merged without repeats");
  SortAscending(&highlights);
  Check(IndexList(highlights) == "1,5,9", "reading order");
  Answer many;
  for (int i = 0; i < 20; i++) {
    many.indices.push_back(100 + i);
  }
  Accumulate(&highlights, many);
  Check(highlights.size() == kMaxHighlights, "capped");
}

}  // namespace

int main() {
  try {
    CheckPageParagraphs();
    CheckPrepare();
    CheckPlan();
    CheckReply();
    CheckAnswer();
    CheckAccumulate();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
