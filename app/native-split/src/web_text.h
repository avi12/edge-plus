// web_text.h — text helpers for pages and scripts. One concern, all of it pure
// (no Edge, no Win32; unit-tested in test/web_text_test.cpp): cut a text on a
// character boundary, say whether a URL is a web page whose text can be read, and
// write a string as a JavaScript literal.
#pragma once
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>

namespace fixedge::web_text {

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

// Web pages and local files; never an edge:// page, an extension, a new tab or a
// blank page.
inline bool IsWebPageUrl(const std::string& url) {
  return StartsWithIgnoreCase(url, "http://") ||
         StartsWithIgnoreCase(url, "https://") ||
         StartsWithIgnoreCase(url, "file://");
}

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

}  // namespace fixedge::web_text
