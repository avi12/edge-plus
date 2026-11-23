// find_bar_text.h — the find bar's text between Edge's UTF-16 views and our
// UTF-8 strings.
#pragma once
#include <string>
#include "edge/api/strings.h"

namespace edgeplus::find {

inline edge::U16View Utf16Of(const std::u16string& text) {
  return edge::U16View{text.data(), text.size()};
}

inline std::string TextOf(const edge::U16View view) {
  return edge::Utf16ToUtf8(view);
}

inline std::u16string Utf16FromUtf8(const std::string& utf8) {
  edge::U16String converted{};
  edge::Utf8ToUtf16(&converted, utf8.c_str());
  const edge::U16View view = edge::U16StringView(&converted);
  const std::u16string text(view.data, view.length);
  edge::FreeU16String(&converted);
  return text;
}

}  // namespace edgeplus::find
