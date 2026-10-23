// web_text.h — text helpers for pages and page scripts. Pure.
#pragma once
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace fixedge::web_text {

[[nodiscard]] inline std::string TruncateUtf8(std::string_view text,
                                              size_t maxBytes) {
  if (text.size() <= maxBytes) {
    return std::string(text);
  }
  size_t end = maxBytes;
  constexpr unsigned char kContinuationMask = 0xC0;
  constexpr unsigned char kContinuationBits = 0x80;
  while (end > 0 && (static_cast<unsigned char>(text[end]) &
                     kContinuationMask) == kContinuationBits) {
    end--;
  }
  return std::string(text.substr(0, end));
}

[[nodiscard]] inline bool StartsWithIgnoreCase(std::string_view text,
                                               std::string_view prefix) {
  for (size_t i = 0; i < prefix.size(); i++) {
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
[[nodiscard]] inline bool IsWebPageUrl(std::string_view url) {
  return StartsWithIgnoreCase(url, "http://") ||
         StartsWithIgnoreCase(url, "https://") ||
         StartsWithIgnoreCase(url, "file://");
}

[[nodiscard]] inline std::string JsStringLiteral(std::string_view utf8) {
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

// Every page script answers `ok US field US ...` or `err US message`.
inline constexpr char kFieldSeparator = '\x1f';
inline constexpr char kOkTag[] = "ok";
inline constexpr char kErrorTag[] = "err";
inline constexpr char kUnreadableAnswer[] =
    "The page gave an answer Edge could not read.";

// A yes/no field of an answer.
inline constexpr char kTrueField[] = "1";
inline constexpr char kFalseField[] = "0";

[[nodiscard]] inline bool IsTrueField(std::string_view field) {
  return field == kTrueField;
}

// An empty text is one empty piece.
[[nodiscard]] inline std::vector<std::string> Split(std::string_view text,
                                                    char separator) {
  std::vector<std::string> pieces;
  size_t start = 0;
  while (true) {
    const size_t end = text.find(separator, start);
    if (end == std::string_view::npos) {
      pieces.emplace_back(text.substr(start));
      return pieces;
    }
    pieces.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
}

[[nodiscard]] inline std::vector<std::string> SplitFields(
    std::string_view answer) {
  return Split(answer, kFieldSeparator);
}

// The Js*Answer helpers write the answer as JavaScript expressions, so a script
// never spells the tags or the separator itself.
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

// The field expression of a JavaScript boolean variable.
[[nodiscard]] inline std::string JsBoolField(std::string_view variable) {
  return std::string(variable) + " ? " + JsStringLiteral(kTrueField) + " : " +
         JsStringLiteral(kFalseField);
}

[[nodiscard]] inline std::string JsErrorAnswerFrom(
    std::string_view messageExpression) {
  std::string answer =
      JsStringLiteral(std::string(kErrorTag) + kFieldSeparator) + " + ";
  answer += messageExpression;
  return answer;
}

[[nodiscard]] inline std::string JsErrorAnswer(std::string_view message) {
  std::string answer = std::string(kErrorTag) + kFieldSeparator;
  answer += message;
  return JsStringLiteral(answer);
}

}  // namespace fixedge::web_text
