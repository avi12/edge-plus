// find_decorations.h — the "/" and "/flags" set around the box while the
// RegExp switch is on.
#pragma once
#include <map>
#include <string>
#include "core/log.h"
#include "edge/api/binding.h"
#include "edge/api/find_bar.h"
#include "edge/api/views.h"
#include "find/find_bar_text.h"
#include "find/find_flags.h"
#include "find/find_options.h"
#include "find/find_text.h"

namespace edgeplus::find {

// With the RegExp switch on, the box reads like a JavaScript literal: "/" before
// the text field and "/" plus the flags after it. They are two plain Labels set
// into the bar's row on either side of the text field, so the caret, selection,
// clipboard and searched text never see them, and the row mirrors them in a
// right-to-left UI. They are found again by where they sit (Edge has no plain
// Label there), so a reloaded DLL takes up the ones it made before.

struct Decorations {
  void* prefix = nullptr;
  void* suffix = nullptr;
};

inline bool TextfieldPlace(void* findBarView, void** row, size_t* index) {
  void* textfield = edge::FindBarTextfield(findBarView);
  void* parent = textfield ? edge::ViewParent(textfield) : nullptr;
  const size_t count = parent ? edge::ViewChildCount(parent) : 0;
  for (size_t i = 0; i < count; i++) {
    if (edge::ViewChildAt(parent, i) == textfield) {
      *row = parent;
      *index = i;
      return true;
    }
  }
  return false;
}

inline void* PlainLabelAt(void* row, size_t index) {
  const bool isInRow = index < edge::ViewChildCount(row);
  void* child = isInRow ? edge::ViewChildAt(row, index) : nullptr;
  return child && edge::IsPlainLabel(child) ? child : nullptr;
}

inline Decorations FindDecorations(void* findBarView) {
  void* row = nullptr;
  size_t index = 0;
  if (!TextfieldPlace(findBarView, &row, &index)) {
    return Decorations{};
  }
  void* prefix = index > 0 ? PlainLabelAt(row, index - 1) : nullptr;
  return Decorations{prefix, PlainLabelAt(row, index + 1)};
}

// The vtables name the children's classes in the PDB: the evidence for where the
// slashes went.
inline void LogRow(void* row) {
  const size_t count = edge::ViewChildCount(row);
  for (size_t i = 0; i < count; i++) {
    Log("[find]   row child %zu vtable %#llx", i,
        static_cast<unsigned long long>(
            edge::VtableRva(edge::ViewChildAt(row, i))));
  }
}

// The box's own font gives the typed text's size and baseline; the count's
// colour is Edge's dimmed secondary text in either theme.
inline void* NewDecorationLabel(void* findBarView) {
  void* countLabel = edge::FindBarCountLabel(findBarView);
  void* label = edge::NewLabelLike(countLabel);
  edge::LabelCopyEnabledColor(label, countLabel);
  edge::LabelUseTextfieldFont(label, edge::FindBarTextfield(findBarView));
  return label;
}

inline Decorations EnsureDecorations(void* findBarView) {
  const Decorations found = FindDecorations(findBarView);
  const bool isComplete =
      (found.prefix != nullptr) && (found.suffix != nullptr);
  const void* countLabel = edge::FindBarCountLabel(findBarView);
  void* row = nullptr;
  size_t index = 0;
  const bool canAdd = !isComplete && (countLabel != nullptr) &&
                      TextfieldPlace(findBarView, &row, &index);
  if (!canAdd) {
    return found;
  }
  if (!found.prefix) {
    edge::AddChildViewAt(row, NewDecorationLabel(findBarView), index);
    index++;
  }
  if (!found.suffix) {
    edge::AddChildViewAt(row, NewDecorationLabel(findBarView), index + 1);
  }
  Log("[find] slash labels added around the box, row %p (vtable %#llx)", row,
      static_cast<unsigned long long>(edge::VtableRva(row)));
  LogRow(row);
  return FindDecorations(findBarView);
}

// So a bar's row is laid out again only on a change.
inline std::map<void*, std::string>& ShownSuffixes() {
  static std::map<void*, std::string> suffixes;
  return suffixes;
}

inline void SyncDecorations(void* findBarView) {
  const bool isShowing = OptionOf(findBarView) == find_text::Option::kRegExp;
  const Decorations decorations =
      isShowing ? EnsureDecorations(findBarView) : FindDecorations(findBarView);
  if (!decorations.prefix || !decorations.suffix) {
    return;
  }
  const find_text::LiteralDecoration literal = find_text::RegExpDecoration(
      TextOf(edge::FindBarText(findBarView)), FlagsOn(findBarView));
  const std::string shown = isShowing ? literal.suffix : std::string();
  std::string& lastShown = ShownSuffixes()[findBarView];
  const bool isVisibilityChanged =
      edge::ViewGetVisible(decorations.prefix) != isShowing;
  if (!isVisibilityChanged && shown == lastShown) {
    return;
  }
  lastShown = shown;
  Log("[find] slashes %s, flag switches on \"%s\"",
      isShowing ? shown.c_str() : "hidden",
      find_flags::Letters(FlagsOn(findBarView)).c_str());
  if (isShowing) {
    edge::LabelSetText(decorations.prefix,
                       Utf16Of(Utf16FromUtf8(literal.prefix)));
    edge::LabelSetText(decorations.suffix, Utf16Of(Utf16FromUtf8(shown)));
  }
  edge::SetViewVisible(decorations.prefix, isShowing);
  edge::SetViewVisible(decorations.suffix, isShowing);
  edge::InvalidateLayout(edge::ViewParent(decorations.prefix));
  edge::FindBarRelayout(findBarView);
}

}  // namespace edgeplus::find
