// find_keys.h — a line break typed into the single-line box with RegExp on:
// Shift+Enter, and a multi-line paste, each typed as \n.
#pragma once
#include <windows.h>
#include <string>
#include "core/handles.h"
#include "edge/api/events.h"
#include "edge/api/strings.h"
#include "find/find_bar_text.h"
#include "find/find_options.h"
#include "find/find_session.h"
#include "find/find_text.h"

namespace edgeplus::find {

// Posted to the window with the keyboard focus, which is the bar's, so the text
// replaces a selection like typing.
inline void TypeIntoBar(const std::string& text) {
  HWND bar = GetFocus();
  if (!bar) {
    return;
  }
  for (const char16_t character : Utf16FromUtf8(text)) {
    PostMessageW(bar, WM_CHAR, character, 1);
  }
}

inline std::string ClipboardText() {
  std::string text;
  const handles::OpenedClipboard clipboard;
  if (!clipboard.IsOpen()) {
    return text;
  }
  const handles::GlobalMemoryLock data{GetClipboardData(CF_UNICODETEXT)};
  const auto* wide = static_cast<const wchar_t*>(data.Data());
  if (wide) {
    text = TextOf(edge::U16View{reinterpret_cast<const char16_t*>(wide),
                                std::char_traits<wchar_t>::length(wide)});
  }
  return text;
}

inline constexpr uint8_t kKeyV = 'V';
inline constexpr uint8_t kKeyInsert = VK_INSERT;

// The box is a single-line text field, so with RegExp on a line break goes in as
// the two characters \n: Shift+Enter (or Ctrl+Enter) types one instead of
// stepping back, and a multi-line paste is typed with each as \n (Edge's own
// paste would drop or flatten them).
inline bool HandleLineBreakKey(void* findBarView, const void* keyEvent) {
  const uint8_t key = edge::PressedKeyCode(keyEvent);
  const bool isRegExpOn = key != 0 && g_isEnabled &&
                          OptionOf(findBarView) == find_text::Option::kRegExp;
  if (!isRegExpOn) {
    return false;
  }
  const bool isShiftDown = GetKeyState(VK_SHIFT) < 0;
  const bool isCtrlDown = GetKeyState(VK_CONTROL) < 0;
  const bool isNewlineKey = key == VK_RETURN && (isShiftDown || isCtrlDown);
  if (isNewlineKey) {
    TypeIntoBar(find_text::kEscapedNewline);
    return true;
  }
  const bool isPasteKey =
      (key == kKeyV && isCtrlDown) || (key == kKeyInsert && isShiftDown);
  if (!isPasteKey) {
    return false;
  }
  const std::string pasted = ClipboardText();
  const bool isMultiline = pasted.find_first_of("\r\n") != std::string::npos;
  if (!isMultiline) {
    return false;
  }
  TypeIntoBar(find_text::EscapeLineBreaks(pasted).substr(
      0, find_text::kMaxPatternChars));
  return true;
}

}  // namespace edgeplus::find
