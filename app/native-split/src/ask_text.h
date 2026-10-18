// ask_text.h — the text side of "Ask" in the find bar. One concern, all
// of it pure (no Edge, no Win32; unit-tested in test/ask_text_test.cpp): the
// page's paragraphs, the prompts that carry them (with the question) to Copilot
// in pieces its message box can take, and the reading of Copilot's reply into
// the paragraphs to highlight. Copilot is only ever asked to name paragraphs by
// label; what it names is checked against the labels that were sent, so a reply
// can never highlight something that was not offered.
#pragma once
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>
#include "find_text.h"
#include "web_text.h"

namespace fixedge::ask_text {

// Paragraphs cross from the page script as one string, between these.
inline constexpr char kParagraphSeparator = '\x1e';

// A paragraph shorter than this is a label or a menu entry, not an answer.
inline constexpr size_t kMinParagraphChars = 12;
// What one paragraph may cost in a prompt; the rest of a long one is cut.
inline constexpr size_t kMaxParagraphChars = 600;
// The paragraphs of one prompt, and the longest question taken.
inline constexpr size_t kChunkChars = 6000;
inline constexpr size_t kMaxQuestionChars = 300;
// A long page is asked about in at most this many prompts, and at most this
// many paragraphs are marked in the end.
inline constexpr size_t kMaxChunks = 4;
inline constexpr size_t kMaxHighlights = 8;
// What Copilot is told it may name per prompt.
inline constexpr int kMaxLabelsPerReply = 5;

inline constexpr char kNoneWord[] = "NONE";

// --- the page's paragraphs -----------------------------------------------------------

struct Paragraph {
  int index = 0;  // its place in the page script's list: what gets highlighted
  std::string text;
};

struct PageParagraphs {
  bool isOk = false;
  std::string error;
  bool isCut = false;  // the page held more text than was read
  std::vector<std::string> texts;
};

// The page script answers `ok US isCut(0|1) US paragraph RS paragraph ...` or
// `err US message`. A paragraph is already one trimmed run of text.
inline PageParagraphs ParsePageParagraphs(const std::string& raw) {
  PageParagraphs page;
  const std::vector<std::string> fields = find_text::SplitFields(raw);
  if (fields[0] == "err") {
    page.error = fields.size() > 1 ? fields[1] : std::string();
    return page;
  }
  constexpr size_t kOkFieldCount = 3;
  const bool isComplete = fields[0] == "ok" && fields.size() == kOkFieldCount;
  if (!isComplete) {
    page.error = "The page gave an answer Edge could not read.";
    return page;
  }
  page.isOk = true;
  page.isCut = fields[1] == "1";
  const std::string& joined = fields[2];
  size_t start = 0;
  while (start <= joined.size() && !joined.empty()) {
    size_t end = joined.find(kParagraphSeparator, start);
    if (end == std::string::npos) {
      end = joined.size();
    }
    page.texts.push_back(joined.substr(start, end - start));
    start = end + 1;
  }
  return page;
}

inline std::string Trim(const std::string& text) {
  size_t start = 0;
  size_t end = text.size();
  while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) {
    start++;
  }
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    end--;
  }
  return text.substr(start, end - start);
}

// Squeeze every run of whitespace (a newline inside a paragraph included) to
// one space, so a label and its text stay on one line of the prompt.
inline std::string Flatten(const std::string& text) {
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

// The paragraphs worth asking about, each cut to what it may cost and keeping
// its place in the page's list.
inline std::vector<Paragraph> Prepare(const std::vector<std::string>& texts) {
  std::vector<Paragraph> kept;
  for (size_t i = 0; i < texts.size(); i++) {
    std::string flat = Flatten(texts[i]);
    if (flat.size() < kMinParagraphChars) {
      continue;
    }
    if (flat.size() > kMaxParagraphChars) {
      flat = web_text::TruncateUtf8(flat, kMaxParagraphChars) + "...";
    }
    kept.push_back(Paragraph{static_cast<int>(i), flat});
  }
  return kept;
}

// --- the prompts -------------------------------------------------------------------------

// A paragraph's label as the prompt shows it. In square brackets so it still
// stands out when the chat's message box drops the line breaks between
// paragraphs (Copilot's editor does with plain text).
inline std::string LabelOf(int index) {
  return "[P" + std::to_string(index) + "]";
}

// One prompt: the question, the rules of the reply, and the paragraphs it
// carries (named by `indices`, in the page's order).
struct Chunk {
  std::vector<int> indices;
  std::string prompt;
};

struct Plan {
  std::vector<Chunk> chunks;
  bool isCut = false;  // paragraphs were left out: more than kMaxChunks fit
};

inline std::string PromptHead(const std::string& question) {
  return "I am searching a web page. Below are paragraphs from it, each "
         "starting with its label in square brackets.\n"
         "Question: \"" +
         question +
         "\"\nReply with the labels of the paragraphs that answer the "
         "question, "
         "most relevant first, separated by commas, like: P3, P7. Use at "
         "most " +
         std::to_string(kMaxLabelsPerReply) +
         " labels. If no paragraph answers it, reply " + kNoneWord +
         ". After the labels, add one short sentence on what you found.\n\n";
}

inline Plan PlanQuestion(const std::string& typedQuestion,
                         const std::vector<Paragraph>& paragraphs) {
  const std::string question =
      Flatten(web_text::TruncateUtf8(typedQuestion, kMaxQuestionChars));
  const std::string head = PromptHead(question);
  Plan plan;
  Chunk current;
  current.prompt = head;
  for (const Paragraph& paragraph : paragraphs) {
    const std::string line =
        LabelOf(paragraph.index) + " " + paragraph.text + "\n";
    const bool isChunkFull =
        !current.indices.empty() &&
        current.prompt.size() - head.size() + line.size() > kChunkChars;
    if (isChunkFull) {
      plan.chunks.push_back(current);
      current = Chunk{};
      current.prompt = head;
      if (plan.chunks.size() == kMaxChunks) {
        plan.isCut = true;
        return plan;
      }
    }
    current.indices.push_back(paragraph.index);
    current.prompt += line;
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
inline Reply ParseReply(const std::string& raw) {
  Reply reply;
  const size_t separator = raw.find(find_text::kFieldSeparator);
  const std::string marker = raw.substr(0, separator);
  const std::string rest = separator == std::string::npos
                               ? std::string()
                               : raw.substr(separator + 1);
  if (marker == "ok") {
    reply.isOk = true;
    reply.text = rest;
  } else if (marker == "err") {
    reply.error = rest;
  } else {
    reply.error = "The chat page gave an answer Edge could not read.";
  }
  return reply;
}

struct Answer {
  std::vector<int> indices;  // the offered paragraphs it named, in its order
  bool isNone = false;       // it said no paragraph answers
  std::string note;          // its sentence on what it found
};

inline bool IsWordChar(char character) {
  return std::isalnum(static_cast<unsigned char>(character)) != 0;
}

// The paragraph number of a label such as "P12" at `start`, or -1: a P (either
// case) not glued to a word before it, then digits not glued to a word after.
// `end` is where the label stops.
inline int LabelAt(const std::string& text, size_t start, size_t* end) {
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

inline bool Contains(const std::vector<int>& numbers, int number) {
  for (const int candidate : numbers) {
    if (candidate == number) {
      return true;
    }
  }
  return false;
}

inline bool IsLabelPunctuation(char character) {
  return std::string("[]().,;:-* \t").find(character) != std::string::npos;
}

// The first sentence of `text`, cut at a line end or at a full stop followed by
// a space, and to kMaxNoteChars at the longest: the reply may run on into the
// page's own chrome (the answer is read from the page's text when its markup is
// not known).
inline constexpr size_t kMaxNoteChars = 200;

inline std::string FirstSentence(const std::string& text) {
  size_t end = text.find('\n');
  if (end == std::string::npos) {
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

// What Copilot said after (or instead of) its labels: its first sentence
// without the leading labels.
inline std::string NoteOf(const std::string& reply) {
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

// Read Copilot's reply to one prompt. Only labels among `offered` count; a label
// it invented, or from another prompt, is ignored.
inline Answer ParseAnswer(const std::string& reply,
                          const std::vector<int>& offered) {
  Answer answer;
  for (size_t i = 0; i < reply.size(); i++) {
    size_t end = 0;
    const int number = LabelAt(reply, i, &end);
    if (number < 0) {
      continue;
    }
    i = end - 1;
    const bool isNew =
        Contains(offered, number) && !Contains(answer.indices, number);
    if (isNew &&
        answer.indices.size() < static_cast<size_t>(kMaxLabelsPerReply)) {
      answer.indices.push_back(number);
    }
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

// Add what one reply named to the paragraphs to highlight: no repeats, at most
// kMaxHighlights in all.
inline void Accumulate(std::vector<int>* highlights, const Answer& answer) {
  for (const int index : answer.indices) {
    const bool canAdd =
        highlights->size() < kMaxHighlights && !Contains(*highlights, index);
    if (canAdd) {
      highlights->push_back(index);
    }
  }
}

// The paragraphs in the page's reading order (the bar steps through them top to
// bottom whatever order Copilot ranked them in).
inline void SortAscending(std::vector<int>* indices) {
  for (size_t i = 1; i < indices->size(); i++) {
    for (size_t j = i; j > 0 && (*indices)[j - 1] > (*indices)[j]; j--) {
      const int swapped = (*indices)[j];
      (*indices)[j] = (*indices)[j - 1];
      (*indices)[j - 1] = swapped;
    }
  }
}

// The paragraph numbers as the highlight script takes them: "3,7,12".
inline std::string IndexList(const std::vector<int>& indices) {
  std::string list;
  for (size_t i = 0; i < indices.size(); i++) {
    list += (i == 0 ? "" : ",") + std::to_string(indices[i]);
  }
  return list;
}

}  // namespace fixedge::ask_text
