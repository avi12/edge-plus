// ask_text.h — the text side of "Ask" in the find bar: the page's sentences,
// the prompts that carry them to Copilot, and reading its reply into sentences
// to highlight. Pure; tested in test/ask_text_test.cpp. Copilot only names
// sentences by label, checked against the labels sent, so a reply can never
// highlight something that was not offered.
#pragma once
#include <cctype>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "find/find_text.h"
#include "find/web_text.h"

namespace edgeplus::ask_text {

// Paragraphs cross from the page script as one string, between these; the
// sentences of one paragraph are between the second.
inline constexpr char kParagraphSeparator = '\x1e';
inline constexpr char kSentenceSeparator = '\x1d';

// A paragraph shorter than this is a label or a menu entry, not an answer.
inline constexpr size_t kMinParagraphChars = 12;
// What one paragraph may cost in a prompt; its later sentences are left out and
// a longer sentence is cut.
inline constexpr size_t kMaxParagraphChars = 600;
inline constexpr size_t kChunkChars = 6000;
inline constexpr size_t kMaxQuestionChars = 300;
inline constexpr size_t kMaxChunks = 4;
// A question about a whole list (the features, the steps) names every item.
inline constexpr size_t kMaxHighlights = 60;
inline constexpr int kMaxLabelsPerReply = 60;

inline constexpr char kNoneWord[] = "NONE";

// --- the page's sentences ------------------------------------------------------------

struct Sentence {
  int index = 0;  // its place in the page script's list: what gets highlighted
  int paragraph = 0;
  std::string text;
};

struct PageParagraphs {
  bool isOk = false;
  std::string error;
  bool isCut = false;              // the page held more text than was read
  std::vector<std::string> texts;  // every sentence of the page, in order
  std::vector<int> paragraphs;     // the paragraph each of them belongs to
};

// The page script answers `ok US isCut(0|1) US paragraph RS paragraph ...` or
// `err US message`, a paragraph being its trimmed sentences between GS.
[[nodiscard]] inline PageParagraphs ParsePageParagraphs(std::string_view raw) {
  PageParagraphs page;
  const std::vector<std::string> fields = web_text::SplitFields(raw);
  if (fields[0] == web_text::kErrorTag) {
    page.error = fields.size() > 1 ? fields[1] : std::string();
    return page;
  }
  constexpr size_t kOkFieldCount = 3;
  const bool isComplete =
      fields[0] == web_text::kOkTag && fields.size() == kOkFieldCount;
  if (!isComplete) {
    page.error = web_text::kUnreadableAnswer;
    return page;
  }
  page.isOk = true;
  page.isCut = web_text::IsTrueField(fields[1]);
  const std::string& joined = fields[2];
  if (joined.empty()) {
    return page;
  }
  const std::vector<std::string> paragraphs =
      web_text::Split(joined, kParagraphSeparator);
  for (size_t paragraph = 0; paragraph < paragraphs.size(); paragraph++) {
    for (const std::string& sentence :
         web_text::Split(paragraphs[paragraph], kSentenceSeparator)) {
      page.texts.push_back(sentence);
      page.paragraphs.push_back(static_cast<int>(paragraph));
    }
  }
  return page;
}

[[nodiscard]] inline std::string Trim(std::string_view text) {
  size_t start = 0;
  size_t end = text.size();
  while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) {
    start++;
  }
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    end--;
  }
  return std::string(text.substr(start, end - start));
}

// So a label and its text stay on one line of the prompt.
[[nodiscard]] inline std::string Flatten(std::string_view text) {
  std::string flat;
  bool isPendingSpace = false;
  for (const char character : text) {
    if (std::isspace(static_cast<unsigned char>(character))) {
      isPendingSpace = !flat.empty();
    } else {
      if (isPendingSpace) {
        flat.push_back(' ');
      }
      isPendingSpace = false;
      flat.push_back(character);
    }
  }
  return flat;
}

// `paragraphs` says which paragraph each text belongs to; without it every text
// is a paragraph of its own.
[[nodiscard]] inline int ParagraphOf(const std::vector<int>& paragraphs,
                                     size_t index) {
  return index < paragraphs.size() ? paragraphs[index]
                                   : static_cast<int>(index);
}

inline void AppendParagraph(const std::vector<std::string>& texts, size_t first,
                            size_t last, int paragraph,
                            std::vector<Sentence>* kept) {
  std::vector<std::string> flats;
  size_t paragraphChars = 0;
  for (size_t i = first; i < last; i++) {
    flats.push_back(Flatten(texts[i]));
    paragraphChars += flats.back().size();
  }
  if (paragraphChars < kMinParagraphChars) {
    return;
  }
  size_t keptChars = 0;
  for (size_t i = first; i < last && keptChars < kMaxParagraphChars; i++) {
    std::string& flat = flats[i - first];
    if (flat.empty()) {
      continue;
    }
    if (flat.size() > kMaxParagraphChars) {
      flat = web_text::TruncateUtf8(flat, kMaxParagraphChars) + "...";
    }
    keptChars += flat.size();
    kept->push_back(Sentence{static_cast<int>(i), paragraph, flat});
  }
}

[[nodiscard]] inline std::vector<Sentence> Prepare(
    const std::vector<std::string>& texts,
    const std::vector<int>& paragraphs = {}) {
  std::vector<Sentence> kept;
  size_t first = 0;
  while (first < texts.size()) {
    const int paragraph = ParagraphOf(paragraphs, first);
    size_t last = first + 1;
    while (last < texts.size() && ParagraphOf(paragraphs, last) == paragraph) {
      last++;
    }
    AppendParagraph(texts, first, last, paragraph, &kept);
    first = last;
  }
  return kept;
}

// --- the prompts -------------------------------------------------------------------------

// In square brackets so it still stands out when Copilot's message box drops
// the line breaks between paragraphs, as it does with plain text.
[[nodiscard]] inline std::string LabelOf(int index) {
  return "[P" + std::to_string(index) + "]";
}

struct Chunk {
  std::vector<int> indices;
  std::string prompt;
};

struct Plan {
  std::vector<Chunk> chunks;
  bool isCut = false;  // paragraphs were left out: more than kMaxChunks fit
};

[[nodiscard]] inline std::string PromptHead(std::string_view question) {
  return "I am searching a web page. Below is text from it, one paragraph per "
         "line, each sentence starting with its label in square brackets.\n"
         "Question: \"" +
         std::string(question) +
         "\"\nReply with the labels of every sentence that is part of the "
         "answer, separated by commas, like: P3, P7. A question about a whole "
         "list (the features, the steps, the prices) needs every item of it. "
         "Name just the sentences that belong to the answer, not unrelated "
         "ones beside them; they may be in different paragraphs, and a run of "
         "neighbouring sentences can be written P3-P9. Use at most " +
         std::to_string(kMaxLabelsPerReply) +
         " labels (a run counts each of its sentences). If no sentence "
         "answers it, reply " +
         kNoneWord +
         ". After the labels, add one short sentence on what you found.\n\n";
}

[[nodiscard]] inline Plan PlanQuestion(std::string_view typedQuestion,
                                       const std::vector<Sentence>& sentences) {
  const std::string question =
      Flatten(web_text::TruncateUtf8(typedQuestion, kMaxQuestionChars));
  const std::string head = PromptHead(question);
  Plan plan;
  Chunk current;
  current.prompt = head;
  int previousParagraph = -1;
  for (const Sentence& sentence : sentences) {
    const std::string labelled = LabelOf(sentence.index) + " " + sentence.text;
    const bool isChunkFull =
        !current.indices.empty() &&
        current.prompt.size() - head.size() + labelled.size() + 1 > kChunkChars;
    if (isChunkFull) {
      plan.chunks.push_back(current);
      current = Chunk{};
      current.prompt = head;
      previousParagraph = -1;
      if (plan.chunks.size() == kMaxChunks) {
        plan.isCut = true;
        return plan;
      }
    }
    const bool isSameParagraph = sentence.paragraph == previousParagraph;
    if (isSameParagraph) {
      current.prompt.back() = ' ';
    }
    previousParagraph = sentence.paragraph;
    current.indices.push_back(sentence.index);
    current.prompt += labelled + "\n";
  }
  if (!current.indices.empty()) {
    plan.chunks.push_back(current);
  }
  return plan;
}

// --- Copilot's reply ---------------------------------------------------------------------

struct Reply {
  bool isOk = false;
  std::string text;
  std::string error;
};

// The Copilot script answers `ok US reply text` or `err US message`.
[[nodiscard]] inline Reply ParseReply(std::string_view raw) {
  Reply reply;
  const size_t separator = raw.find(web_text::kFieldSeparator);
  const std::string_view marker = raw.substr(0, separator);
  const std::string_view rest = separator == std::string_view::npos
                                    ? std::string_view()
                                    : raw.substr(separator + 1);
  if (marker == web_text::kOkTag) {
    reply.isOk = true;
    reply.text = rest;
  } else if (marker == web_text::kErrorTag) {
    reply.error = rest;
  } else {
    reply.error = "The chat page gave an answer Edge could not read.";
  }
  return reply;
}

struct Answer {
  std::vector<int> indices;  // the offered sentences it named, in its order
  bool isNone = false;       // it said no sentence answers
  std::string note;          // its sentence on what it found
};

[[nodiscard]] inline bool IsWordChar(char character) {
  return std::isalnum(static_cast<unsigned char>(character)) != 0;
}

// The number of a label such as "P12" at `start`, or -1. `end` is where the
// label stops.
[[nodiscard]] inline int LabelAt(std::string_view text, size_t start,
                                 size_t* end) {
  const bool isP =
      start < text.size() && (text[start] == 'P' || text[start] == 'p');
  const bool isWordStart = start == 0 || !IsWordChar(text[start - 1]);
  if (!isP || !isWordStart) {
    return -1;
  }
  size_t cursor = start + 1;
  int number = 0;
  constexpr size_t kMaxDigits = 6;
  while (cursor < text.size() && cursor - start - 1 < kMaxDigits &&
         std::isdigit(static_cast<unsigned char>(text[cursor]))) {
    number = number * 10 + (text[cursor] - '0');
    cursor++;
  }
  const bool hasDigits = cursor > start + 1;
  const bool isWordEnd = cursor >= text.size() || !IsWordChar(text[cursor]);
  if (!hasDigits || !isWordEnd) {
    return -1;
  }
  *end = cursor;
  return number;
}

[[nodiscard]] inline constexpr bool Contains(std::span<const int> numbers,
                                             int number) noexcept {
  for (const int candidate : numbers) {
    if (candidate == number) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline constexpr bool IsLabelPunctuation(
    char character) noexcept {
  return std::string_view("[]().,;:-* \t").find(character) !=
         std::string_view::npos;
}

// Capped because the reply may run on into the page's own chrome (it is read
// from the page's text when its markup is not known).
inline constexpr size_t kMaxNoteChars = 200;

[[nodiscard]] inline std::string FirstSentence(std::string_view text) {
  size_t end = text.find('\n');
  if (end == std::string_view::npos) {
    end = text.size();
  }
  for (size_t i = 0; i + 1 < end; i++) {
    const bool isStop = text[i] == '.' || text[i] == '!' || text[i] == '?';
    if (isStop && text[i + 1] == ' ') {
      end = i + 1;
      break;
    }
  }
  std::string sentence = Trim(text.substr(0, end));
  if (sentence.size() > kMaxNoteChars) {
    sentence = web_text::TruncateUtf8(sentence, kMaxNoteChars) + "...";
  }
  return sentence;
}

// The reply's first sentence after its leading labels.
[[nodiscard]] inline std::string NoteOf(std::string_view reply) {
  const std::string text = Trim(reply);
  size_t cursor = 0;
  while (cursor < text.size()) {
    size_t labelEnd = 0;
    if (IsLabelPunctuation(text[cursor])) {
      cursor++;
    } else if (LabelAt(text, cursor, &labelEnd) >= 0) {
      cursor = labelEnd;
    } else {
      break;
    }
  }
  return FirstSentence(Trim(text.substr(cursor)));
}

// The label that closes a run such as "P3-P5" (a hyphen or dash between two
// labels), or -1 when the label at `end` is not the start of one.
[[nodiscard]] inline int RunEndAt(std::string_view text, size_t end,
                                  size_t* runEnd) {
  size_t cursor = end;
  while (cursor < text.size() && text[cursor] == ' ') {
    cursor++;
  }
  const std::string_view dashes[] = {"-", "\xE2\x80\x93", "\xE2\x80\x94"};
  size_t dashLength = 0;
  for (const std::string_view dash : dashes) {
    if (text.substr(cursor, dash.size()) == dash) {
      dashLength = dash.size();
    }
  }
  if (dashLength == 0) {
    return -1;
  }
  cursor += dashLength;
  while (cursor < text.size() && text[cursor] == ' ') {
    cursor++;
  }
  return LabelAt(text, cursor, runEnd);
}

// A label it invented, or from another prompt, is ignored. A run P3-P5 names
// every offered sentence from 3 to 5.
[[nodiscard]] inline Answer ParseAnswer(std::string_view reply,
                                        const std::vector<int>& offered) {
  Answer answer;
  const auto add = [&](int number) {
    const bool isNew =
        Contains(offered, number) && !Contains(answer.indices, number);
    if (isNew &&
        answer.indices.size() < static_cast<size_t>(kMaxLabelsPerReply)) {
      answer.indices.push_back(number);
    }
  };
  for (size_t i = 0; i < reply.size(); i++) {
    size_t end = 0;
    const int number = LabelAt(reply, i, &end);
    if (number < 0) {
      continue;
    }
    i = end - 1;
    size_t runEnd = 0;
    const int last = RunEndAt(reply, end, &runEnd);
    const bool isRun = last > number;
    if (!isRun) {
      add(number);
      continue;
    }
    for (int member = number; member <= last; member++) {
      add(member);
    }
    i = runEnd - 1;
  }
  const std::string head = Trim(reply).substr(0, sizeof(kNoneWord) - 1);
  bool isHeadNone = head.size() == sizeof(kNoneWord) - 1;
  for (size_t i = 0; isHeadNone && i < head.size(); i++) {
    isHeadNone =
        std::toupper(static_cast<unsigned char>(head[i])) == kNoneWord[i];
  }
  answer.isNone = answer.indices.empty() && isHeadNone;
  answer.note = NoteOf(reply);
  return answer;
}

inline void Accumulate(std::vector<int>* highlights, const Answer& answer) {
  for (const int index : answer.indices) {
    const bool canAdd =
        highlights->size() < kMaxHighlights && !Contains(*highlights, index);
    if (canAdd) {
      highlights->push_back(index);
    }
  }
}

// The bar steps through them top to bottom, whatever order Copilot ranked them.
inline void SortAscending(std::vector<int>* indices) {
  for (size_t i = 1; i < indices->size(); i++) {
    for (size_t j = i; j > 0 && (*indices)[j - 1] > (*indices)[j]; j--) {
      const int swapped = (*indices)[j];
      (*indices)[j] = (*indices)[j - 1];
      (*indices)[j - 1] = swapped;
    }
  }
}

// "3,7,12"
[[nodiscard]] inline std::string IndexList(std::span<const int> indices) {
  std::string list;
  for (size_t i = 0; i < indices.size(); i++) {
    list += (i == 0 ? "" : ",") + std::to_string(indices[i]);
  }
  return list;
}

}  // namespace edgeplus::ask_text
