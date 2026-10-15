// channel.h — the DLL's command channel into Edge's UI thread. One concern: the
// browser window it rides on, the registered message, and the codes posted
// through it. control.h subclasses the window and runs the codes; hooks post
// work that must wait until Edge's current event is done; scripts post the
// same numbers.
#pragma once
#include <windows.h>
#include "pane_tree.h"

namespace fixedge::channel {

inline HWND g_window = nullptr;  // the subclassed browser window
inline UINT g_message = 0;       // the registered kMessageName

inline constexpr wchar_t kMessageName[] = L"fixedge.addpane";

// The codes: the wParam of a kMessageName message. A `...Base` starts a range
// and the code's offset from it is the operand. A code no command claims falls
// through: from kCreatePanesBase up it creates a split of (code -
// kCreatePanesBase) panes, from kCreateSplit up a 2-pane split, and below
// kCreateSplit it grows the split by one pane.
enum class Command : WPARAM {
  kCreateSplit = 100,      // Edge's proven 2-pane AddToNewSplit
  kCreatePanesBase = 200,  // + N: N panes, directly through AddToSplitImpl
  kAddPaneBase = 300,      // + Direction: add a pane beside the active one
  kSwap = 400,             // swap the active pane with its counterpart
  kShowBarBase = 500,      // + i: show pane i's hover bar
  kEnsureGrips = 700,      // give every pane of a grown split its drag grip
  kTeardown = 701,         // undo everything UI-side before this DLL unloads
  kMenuArrowBase = 710,    // + Direction: a ⋯ d-pad arrow was clicked
  kTabDrop = 720,          // a tab Alt-dragged onto the split: add it
  kLinkPane = 721,         // a pane's link opened a tab: make it a pane
  kCloseOfferPane = 722,   // a grown split's tab offer was dismissed
  kApplyTheme = 800,       // apply the theme for the current OS light/dark mode
};

// The width of each range: one code per Direction (kUp..kRight), and the
// panes whose hover bar can be shown. A code must stay clear of every range
// below it (a range of directions spans kDirectionCount codes).
inline constexpr WPARAM kDirectionCount =
    static_cast<WPARAM>(Direction::kRight) + 1;
inline constexpr WPARAM kShowBarPaneLimit = 16;

inline constexpr WPARAM CodeOf(Command command) {
  return static_cast<WPARAM>(command);
}

// Every range ends before the next code starts.
static_assert(CodeOf(Command::kAddPaneBase) + kDirectionCount <=
                  CodeOf(Command::kSwap),
              "the add-pane range overlaps the next code");
static_assert(CodeOf(Command::kShowBarBase) + kShowBarPaneLimit <=
                  CodeOf(Command::kEnsureGrips),
              "the show-bar range overlaps the next code");
static_assert(CodeOf(Command::kMenuArrowBase) + kDirectionCount <=
                  CodeOf(Command::kTabDrop),
              "the menu-arrow range overlaps the next code");

// The code for `direction` in a range of one code per direction.
inline constexpr WPARAM CodeOf(Command base, Direction direction) {
  return CodeOf(base) + static_cast<WPARAM>(direction);
}

// Whether `wparam` is one of the `count` codes that start at `base`.
inline bool IsInRange(WPARAM wparam, Command base, WPARAM count) {
  return wparam >= CodeOf(base) && wparam < CodeOf(base) + count;
}

// The operand of a ranged code (its offset from `base`).
inline int OperandOf(WPARAM wparam, Command base) {
  return static_cast<int>(wparam) - static_cast<int>(CodeOf(base));
}

// Queue `code` to run on the UI thread once the current event is done.
inline void Post(WPARAM code) {
  const bool isOpen = g_window && g_message != 0;
  if (isOpen) {
    PostMessageW(g_window, g_message, code, 0);
  }
}

}  // namespace fixedge::channel
