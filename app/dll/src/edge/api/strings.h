// strings.h — the libc++ strings Edge's functions take and return: views over
// our characters, a std::u16string and a std::string read or built in place,
// and UTF-8 to UTF-16 through Edge's own conversion.
#pragma once
#include <cstdint>
#include <cstring>
#include <string_view>
#include <string>
#include "edge/api/binding.h"
#include "edge/api/memory.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

struct U8View {
  const char* data = nullptr;
  uint64_t len = 0;
};
static_assert(sizeof(U8View) == 16, "a std::string_view is {data, size}");
struct U16String {
  uint64_t _[3] = {};
};
static_assert(sizeof(U16String) == 24, "a libc++ std::u16string is 24 bytes");
// base::UTF8ToUTF16(out /*rcx*/, string_view{ptr,len} /*rdx*/) -> fills `out`.
inline void Utf8ToUtf16(U16String* out, const U8View* view) {
  using Fn = void*(__fastcall*)(void*, const void*);
  Callable<Fn>(edge155::rva::UTF8ToUTF16)(out, view);
}
inline void Utf8ToUtf16(U16String* out, const char* utf8) {
  const U8View view{utf8, strlen(utf8)};
  Utf8ToUtf16(out, &view);
}

struct U16View {
  const char16_t* data = nullptr;
  uint64_t length = 0;
};
static_assert(sizeof(U16View) == 16, "a std::u16string_view is {data, size}");
inline U16View U16StringView(const void* string) {
  const auto* bytes = static_cast<const uint8_t*>(string);
  const bool isLong = (bytes[edge155::off::U16STRING_short_size] &
                       edge155::off::U16STRING_long_flag) != 0;
  if (isLong) {
    return U16View{*reinterpret_cast<const char16_t* const*>(bytes),
                   *reinterpret_cast<const size_t*>(
                       bytes + edge155::off::U16STRING_long_size)};
  }
  return U16View{reinterpret_cast<const char16_t*>(bytes),
                 bytes[edge155::off::U16STRING_short_size]};
}

// A libc++ std::string in its long form (pointer, size, capacity with the top
// bit set), over characters that outlive the call: enough for a callee that only
// reads or copies it.
struct StdStringView {
  const char* data = nullptr;
  uint64_t size = 0;
  uint64_t capacityAndFlag = 0;
};
static_assert(sizeof(StdStringView) == 24, "a libc++ std::string is 24 bytes");

inline StdStringView MakeStdStringView(const std::string& text) {
  constexpr uint64_t kLongFlag = uint64_t{1} << 63;
  return StdStringView{text.c_str(), text.size(),
                       static_cast<uint64_t>(text.size()) | kLongFlag};
}

// A libc++ std::string's characters (the same layout as the u16string above).
inline std::string StdStringText(const void* string) {
  const auto* bytes = static_cast<const uint8_t*>(string);
  const bool isLong = (bytes[edge155::off::U16STRING_short_size] &
                       edge155::off::U16STRING_long_flag) != 0;
  if (isLong) {
    return std::string(*reinterpret_cast<const char* const*>(bytes),
                       *reinterpret_cast<const size_t*>(
                           bytes + edge155::off::U16STRING_long_size));
  }
  return std::string(reinterpret_cast<const char*>(bytes),
                     bytes[edge155::off::U16STRING_short_size]);
}

inline std::string Utf16ToUtf8(U16View text) {
  if (text.length == 0) {
    return std::string();
  }
  const int length = static_cast<int>(text.length);
  const int byteCount = WideCharToMultiByte(
      CP_UTF8, 0, reinterpret_cast<const wchar_t*>(text.data), length, nullptr,
      0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(byteCount), '\0');
  WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(text.data),
                      length, utf8.data(), byteCount, nullptr, nullptr);
  return utf8;
}

// Free a u16string we made with Utf8ToUtf16 (its text is on Edge's heap when
// it outgrew the inline buffer).
inline void FreeU16String(U16String* string) {
  auto* bytes = reinterpret_cast<uint8_t*>(string);
  const bool isLong = (bytes[edge155::off::U16STRING_short_size] &
                       edge155::off::U16STRING_long_flag) != 0;
  if (isLong) {
    EdgeDelete(*reinterpret_cast<void**>(bytes));
  }
}

// A u16string made from UTF-8 for a call into Edge, freed when it goes out of
// scope.
struct OwnedU16String {
  explicit OwnedU16String(std::string_view utf8) {
    const U8View view{utf8.data(), utf8.size()};
    Utf8ToUtf16(&text, &view);
  }
  ~OwnedU16String() {
    FreeU16String(&text);
  }
  OwnedU16String(const OwnedU16String&) = delete;
  OwnedU16String& operator=(const OwnedU16String&) = delete;
  OwnedU16String(OwnedU16String&&) = delete;
  OwnedU16String& operator=(OwnedU16String&&) = delete;

  U16String text{};
};

}  // namespace edgeplus::edge
