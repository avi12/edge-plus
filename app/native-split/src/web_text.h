// web_text.h — text helpers for pages and scripts. One concern, all of it pure
// (no Edge, no Win32; unit-tested in test/web_text_test.cpp): cut a text on a
// character boundary, say whether a URL is a web page whose text can be read,
// write a string as a JavaScript literal, and write and split a page script's
// answer.
#pragma once
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace fixedge::web_text {

// Cut `text` to at most `maxBytes` without splitting a UTF-8 character.
[[nodiscard]] inline std::string TruncateUtf8(const std::string& text,
                                              size_t maxBytes) {
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

[[nodiscard]] inline bool StartsWithIgnoreCase(const std::string& text,
                                               const char* prefix) {
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

// Web pages and local files; never an edge:// page, an extension, a new tab or a
// blank page.
[[nodiscard]] inline bool IsWebPageUrl(const std::string& url) {
  return StartsWithIgnoreCase(url, "http://") ||
         StartsWithIgnoreCase(url, "https://") ||
         StartsWithIgnoreCase(url, "file://");
}

// `utf8` as a double-quoted JavaScript string literal.
[[nodiscard]] inline std::string JsStringLiteral(const std::string& utf8) {
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

// --- a page script's answer ------------------------------------------------------

// Every page script answers `ok US field US ...` or `err US message`, US being
// the field separator.
inline constexpr char kFieldSeparator = '\x1f';
inline constexpr char kOkTag[] = "ok";
inline constexpr char kErrorTag[] = "err";
inline constexpr char kUnreadableAnswer[] =
    "The page gave an answer Edge could not read.";

// `text` cut at every `separator`; an empty text is one empty piece.
[[nodiscard]] inline std::vector<std::string> Split(const std::string& text,
                                                    char separator) {
  std::vector<std::string> pieces;
  size_t start = 0;
  while (true) {
    const size_t end = text.find(separator, start);
    if (end == std::string::npos) {
      pieces.push_back(text.substr(start));
      return pieces;
    }
    pieces.push_back(text.substr(start, end - start));
    start = end + 1;
  }
}

[[nodiscard]] inline std::vector<std::string> SplitFields(
    const std::string& answer) {
  return Split(answer, kFieldSeparator);
}

// The same answer written as page-script JavaScript, so a script never spells
// the tags or the separator itself.

// A JavaScript expression answering `ok`, then each of `fieldExpressions` (each
// a JavaScript expression) after a separator.
[[nodiscard]] inline std::string JsOkAnswer(
    const std::vector<std::string>& fieldExpressions = {}) {
  if (fieldExpressions.empty()) {
    return JsStringLiteral(kOkTag);
  }
  std::string fields = "[" + JsStringLiteral(kOkTag);
  for (const std::string& field : fieldExpressions) {
    fields += ", " + field;
  }
  return fields + "].join(" + JsStringLiteral(std::string(1, kFieldSeparator)) +
         ")";
}

// A JavaScript expression answering `err` with the message the JavaScript
// expression `messageExpression` gives.
[[nodiscard]] inline std::string JsErrorAnswerFrom(
    const std::string& messageExpression) {
  return JsStringLiteral(std::string(kErrorTag) + kFieldSeparator) + " + " +
         messageExpression;
}

// A JavaScript expression answering `err` with the fixed text `message`.
[[nodiscard]] inline std::string JsErrorAnswer(const std::string& message) {
  return JsStringLiteral(std::string(kErrorTag) + kFieldSeparator + message);
}

}  // namespace fixedge::web_text
