// find_bar.h — Edge's native find bar (FindBarView): its text box, its count
// label, and a label of ours styled like it.
#pragma once
#include <cstdint>
#include <cstring>
#include "edge/api/binding.h"
#include "edge/api/strings.h"
#include "edge/api/views.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// A new, empty views::Label styled exactly like `model` (the same text context
// and style, so the same font and the same theme colour ids), not yet in any
// view. Its parent owns it once added.
inline void* NewLabelLike(void* model) {
  void* label = NewView(edge155::off::LABEL_allocation_size);
  const U16View empty{nullptr, 0};
  constexpr int kDirectionalityFromText = 0;
  using Fn = void*(__fastcall*)(void*, const U16View*, int, int, int);
  Callable<Fn>(edge155::rva::Label_ctor)(
      label, &empty, Field<int32_t>(model, edge155::off::LABEL_text_context),
      Field<int32_t>(model, edge155::off::LABEL_text_style),
      kDirectionalityFromText);
  return label;
}

// Give `label` the colour `model` was asked to draw in (its SetEnabledColor:
// for the find bar's count, a theme colour id), if it was asked one.
inline void LabelCopyEnabledColor(void* label, void* model) {
  const bool isSet =
      Field<uint8_t>(model,
                     edge155::off::LABEL_requested_enabled_color_is_set) != 0;
  if (!isSet) {
    return;
  }
  using Fn = void(__fastcall*)(void*, uint64_t);
  Callable<Fn>(edge155::rva::Label_SetEnabledColor)(
      label,
      Field<uint64_t>(model, edge155::off::LABEL_requested_enabled_color));
}

// Draw `label` in the font `textfield` draws its text in.
inline void LabelUseTextfieldFont(void* label, void* textfield) {
  using GetFn = const void*(__fastcall*)(void*);
  const void* fontList =
      Callable<GetFn>(edge155::rva::Textfield_GetFontList)(textfield);
  using SetFn = void(__fastcall*)(void*, const void*);
  Callable<SetFn>(edge155::rva::Label_SetFontList)(label, fontList);
}

inline bool IsPlainLabel(void* view) {
  return VtableRva(view) == Rva(edge155::rva::Label_vftable);
}

inline void LabelSetText(void* label, U16View text) {
  using Fn = void(__fastcall*)(void*, const U16View*);
  Callable<Fn>(edge155::rva::Label_SetText)(label, &text);
}

inline void* FindBarTextfield(void* findBarView) {
  return Field<void*>(findBarView, edge155::off::FINDBARVIEW_textfield);
}
inline void* FindBarCountLabel(void* findBarView) {
  return Field<void*>(findBarView, edge155::off::FINDBARVIEW_match_count_label);
}

// The bar re-measures itself (its window follows).
inline void FindBarPreferredSizeChanged(void* findBarView) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::FindBarView_PreferredSizeChanged)(findBarView);
}

// The bar's own row changed children: lay it out again.
inline void FindBarRelayout(void* findBarView) {
  InvalidateLayout(findBarView);
  FindBarPreferredSizeChanged(findBarView);
}

// What the bar's text field holds now (a view of the field's own storage, valid
// until the field next changes).
inline U16View FindBarText(void* findBarView) {
  U16View text{nullptr, 0};
  using Fn = void*(__fastcall*)(void*, U16View*);
  Callable<Fn>(edge155::rva::Textfield_GetText)(
      Field<void*>(findBarView, edge155::off::FINDBARVIEW_textfield), &text);
  return text;
}

// Show "ordinal/matches" in the bar's count label and enable its arrow buttons,
// as Edge does for its own search's result. `findText` must be what the text
// field holds: the bar rewrites the field when they differ.
inline void FindBarShowResult(void* findBarView, int matches, int ordinal,
                              U16View findText) {
  static int sRequestId = 0;
  uint8_t details[edge155::off::FINDDETAILS_size] = {};
  // The bar skips a result equal to the one it last showed; ours may follow a
  // status line it knows nothing about, so every result is a new one.
  sRequestId++;
  std::memcpy(details + edge155::off::FINDDETAILS_request_id, &sRequestId,
              sizeof(sRequestId));
  std::memcpy(details + edge155::off::FINDDETAILS_matches, &matches,
              sizeof(matches));
  std::memcpy(details + edge155::off::FINDDETAILS_active_ordinal, &ordinal,
              sizeof(ordinal));
  details[edge155::off::FINDDETAILS_is_final] = 1;
  // A read-only std::u16string in its long form {data, size, capacity|flag}.
  struct LongU16String {
    const char16_t* data = nullptr;
    uint64_t size = 0;
    uint64_t capacityAndFlag = 0;
  };
  const LongU16String text{
      findText.data, findText.length,
      (findText.length + 1) | (size_t{edge155::off::U16STRING_long_flag}
                               << (8 * (sizeof(size_t) - 1)))};
  using Fn = void(__fastcall*)(void*, const void*, const void*);
  Callable<Fn>(edge155::rva::FindBarView_UpdateForResult)(findBarView, details,
                                                          &text);
}

inline void* FindBarLabel(void* findBarView) {
  return Field<void*>(findBarView, edge155::off::FINDBARVIEW_match_count_label);
}

// The FindBarView behind the TextfieldController its key handler is called on.
inline void* FindBarViewOfKeyController(void* controller) {
  return static_cast<uint8_t*>(controller) -
         edge155::off::FINDBARVIEW_textfield_controller;
}

// Put `text` in the bar's count label in place of "n/m" (a short status).
inline void FindBarSetLabel(void* findBarView, U16View text) {
  void* label = FindBarLabel(findBarView);
  using Fn = void(__fastcall*)(void*, const U16View*);
  reinterpret_cast<Fn>(VirtualFunction(
      label, edge155::off::FINDBARLABEL_set_text_vtable_offset))(label, &text);
}

}  // namespace edgeplus::edge
