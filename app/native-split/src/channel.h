// channel.h — the DLL's command channel into Edge's UI thread: hooks post work
// that must wait until Edge's current event is done, and scripts post the same
// codes. control.h subclasses the window and runs them.
#pragma once
#include <windows.h>
#include "pane_tree.h"

namespace fixedge::channel {

inline constinit HWND g_window = nullptr;  // the subclassed browser window
inline constinit UINT g_message = 0;

inline constexpr wchar_t kMessageName[] = L"fixedge.addpane";

// The wParam. A `...Base` starts a range and the offset from it is the operand. A code no command claims falls
// through: from kCreatePanesBase up it creates a split of (code -
// kCreatePanesBase) panes, from kCreateSplit up a 2-pane split, and below
// kCreateSplit it grows the split by one pane.
enum class Command : WPARAM {
  kCreateSplit = 100,        // Edge's proven 2-pane AddToNewSplit
  kCreatePanesBase = 200,    // + N: N panes, directly through AddToSplitImpl
  kAddPaneBase = 300,        // + Direction: add a pane beside the active one
  kSwap = 400,               // swap the active pane with its counterpart
  kShowBarBase = 500,        // + i: show pane i's hover bar
  kEnsureGrips = 700,        // give every pane of a grown split its drag grip
  kTeardown = 701,           // undo everything UI-side before this DLL unloads
  kMenuArrowBase = 710,      // + Direction: a ⋯ d-pad arrow was clicked
  kTabDrop = 720,            // a tab Alt-dragged onto the split: add it
  kLinkPane = 721,           // a pane's link opened a tab: make it a pane
  kCloseOfferPane = 722,     // a grown split's tab offer was dismissed
  kDumpTabRows = 900,        // log the split rows of the vertical tab list
  kForceTabShape = 908,      // cut the tab corners without theming (a script's
                             // stand-in for the theme and its switch)
  kEndPreviewDetach = 909,   // end the preview below
  kPreviewDetachBase = 910,  // + i: the tab list shows pane i out of its split
                             // (a script's stand-in for dragging it there)
  // A script's stand-in for the mouse on a grip (tabdrag.h), each point in
  // window DIPs packed into the lParam (PointOf):
  kGripPressBase = 930,  // + i: press pane i's grip at the point
  kGripMoveTo = 950,     // the held grip's pointer moves to the point
  kGripRelease = 951,    // let go
  kGripEscape = 952,     // Escape
  kApplyTheme = 800,     // apply the theme for the current OS light/dark mode
  kApplyConfig = 801,    // apply the config read from disk, on the UI thread
  kRedrawFollowedPages = 803,  // a theme swap changed a followed page's tone
  kReloadConfig = 802,  // re-read the config file and apply it (a script's
                        // stand-in for the config event, which wakes only
                        // one of the DLLs loaded in the system)
};

inline constexpr WPARAM kDirectionCount =
    static_cast<WPARAM>(Direction::kRight) + 1;
inline constexpr WPARAM kShowBarPaneLimit = 16;

[[nodiscard]] inline constexpr WPARAM CodeOf(Command command) noexcept {
  return static_cast<WPARAM>(command);
}

static_assert(CodeOf(Command::kAddPaneBase) + kDirectionCount <=
                  CodeOf(Command::kSwap),
              "the add-pane range overlaps the next code");
static_assert(CodeOf(Command::kEndPreviewDetach) <
                  CodeOf(Command::kPreviewDetachBase),
              "the preview range starts after the code that ends it");
static_assert(CodeOf(Command::kShowBarBase) + kShowBarPaneLimit <=
                  CodeOf(Command::kEnsureGrips),
              "the show-bar range overlaps the next code");
static_assert(CodeOf(Command::kMenuArrowBase) + kDirectionCount <=
                  CodeOf(Command::kTabDrop),
              "the menu-arrow range overlaps the next code");
static_assert(CodeOf(Command::kPreviewDetachBase) + kShowBarPaneLimit <=
                  CodeOf(Command::kGripPressBase),
              "the preview range overlaps the grip press range");
static_assert(CodeOf(Command::kGripPressBase) + kShowBarPaneLimit <=
                  CodeOf(Command::kGripMoveTo),
              "the grip press range overlaps the next code");

// A point packed into a command's lParam as signed 16-bit x and y.
[[nodiscard]] inline constexpr Point PointOf(LPARAM lparam) noexcept {
  const auto bits = static_cast<uint32_t>(lparam);
  return Point{static_cast<int16_t>(bits & 0xffff),
               static_cast<int16_t>(bits >> 16)};
}

[[nodiscard]] inline constexpr WPARAM CodeOf(Command base,
                                             Direction direction) noexcept {
  return CodeOf(base) + static_cast<WPARAM>(direction);
}

[[nodiscard]] inline constexpr bool IsInRange(WPARAM wparam, Command base,
                                              WPARAM count) noexcept {
  return wparam >= CodeOf(base) && wparam < CodeOf(base) + count;
}

[[nodiscard]] inline constexpr int OperandOf(WPARAM wparam,
                                             Command base) noexcept {
  return static_cast<int>(wparam) - static_cast<int>(CodeOf(base));
}

inline void Post(WPARAM code) {
  const bool isOpen = g_window && g_message != 0;
  if (isOpen) {
    PostMessageW(g_window, g_message, code, 0);
  }
}

// The WM_TIMER ids on the subclassed browser window: one closed set, so no two
// modules can pick the same id.
enum class WindowTimer : UINT_PTR {
  kFindPoll = 0xF1EC,        // find.h: the find bar's poll
  kAnimationFrame = 0xF1ED,  // anim.h: a pane transition's next frame
  kThemeRepaint = 0xF1EE,    // theme.h: the frame redraw after a theme swap
  kRenderNudge = 0xF1EF,     // repaint.h: the settled panes' paint nudge
  kReadingPoll = 0xF1F1,     // reading_mode.h: the visit to the reading views
};

[[nodiscard]] inline constexpr UINT_PTR TimerIdOf(WindowTimer timer) noexcept {
  return static_cast<UINT_PTR>(timer);
}

}  // namespace fixedge::channel
