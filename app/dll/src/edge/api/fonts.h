// fonts.h — a label's font: the gfx::FontList it draws in, its size, and one
// made from a family name and a size.
#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include "edge/api/binding.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// A gfx::FontList, and a gfx::Font: each is one scoped_refptr, so two holding
// the same impl are the same font.
struct FontHandle {
  const void* impl = nullptr;
};
static_assert(sizeof(FontHandle) == 8, "a FontList is one scoped_refptr");

// A libc++ std::string short enough for its inline buffer: the chars, then the
// size in the last byte with its top bit (the long flag) clear, the layout
// TabTitle::TabTitle reads a u16string's in. It owns no heap, so it needs no
// destructor.
struct ShortString {
  static constexpr size_t kCapacity = 22;
  char chars[kCapacity + 1] = {};
  uint8_t size = 0;
};
static_assert(sizeof(ShortString) == 24, "a libc++ std::string is 24 bytes");

[[nodiscard]] inline std::optional<ShortString> MakeShortString(
    std::string_view text) {
  if (text.size() > ShortString::kCapacity) {
    return std::nullopt;
  }
  ShortString shortString;
  memcpy(shortString.chars, text.data(), text.size());
  shortString.size = static_cast<uint8_t>(text.size());
  return shortString;
}

// The font a label draws its text in (its RenderText's); null before it has
// a RenderText.
inline const FontHandle* LabelFontList(void* label) {
  void* const renderText = Field<void*>(label, edge155::off::LABEL_render_text);
  if (!renderText) {
    return nullptr;
  }
  return &Field<FontHandle>(renderText, edge155::off::RENDERTEXT_font_list);
}

inline int FontListPixelSize(const FontHandle& fontList) {
  using Fn = int(__fastcall*)(const FontHandle*);
  return Callable<Fn>(edge155::rva::FontList_GetFontSize)(&fontList);
}

// A reference of our own to `fontList`, kept until the process ends.
inline FontHandle CopyFontList(const FontHandle& fontList) {
  FontHandle copy;
  using Fn = void*(__fastcall*)(FontHandle*, const FontHandle*);
  Callable<Fn>(edge155::rva::FontList_copy_ctor)(&copy, &fontList);
  return copy;
}

// A font list of the one family at `pixelSize`, kept until the process ends
// (the Font it is made from too: a few bytes). A family Windows lacks comes
// back as its default font.
inline FontHandle MakeFontList(const ShortString& family, int pixelSize) {
  FontHandle font;
  using FontFn = void*(__fastcall*)(FontHandle*, const ShortString*, int);
  Callable<FontFn>(edge155::rva::Font_ctor_name_size)(&font, &family,
                                                      pixelSize);
  FontHandle fontList;
  using ListFn = void*(__fastcall*)(FontHandle*, const FontHandle*);
  Callable<ListFn>(edge155::rva::FontList_ctor_font)(&fontList, &font);
  return fontList;
}

// Through the label's own virtual, as EdgeTab::EdgeTab sets the title's font.
inline void LabelSetFontList(void* label, const FontHandle& fontList) {
  using Fn = void(__fastcall*)(void*, const FontHandle*);
  reinterpret_cast<Fn>(VirtualFunction(
      label, edge155::off::LABEL_vtbl_SetFontList))(label, &fontList);
}

}  // namespace edgeplus::edge
