// find_detours.h — the detours on Edge's find bar: a search, the next and
// previous match, a key, the options view and its toggles, a switch of tab
// and the end of a find session. Anything but a RegExp or a question passes
// to Edge's own find untouched.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "edge/api/events.h"
#include "edge/api/find_bar.h"
#include "edge/api/find_bar_toggles.h"
#include "edge/api/strings.h"
#include "edge/offsets.h"
#include "find/find_ask.h"
#include "find/find_bar_session.h"
#include "find/find_bar_text.h"
#include "find/find_bar_width.h"
#include "find/find_decorations.h"
#include "find/find_flags.h"
#include "find/find_keys.h"
#include "find/find_options.h"
#include "find/find_search.h"
#include "find/find_session.h"
#include "find/find_status.h"
#include "find/find_text.h"
#include "find/find_toggles.h"
#include "find/tab_list.h"

namespace edgeplus::find {

using Find_t = void(__fastcall*)(void*, const edge::U16View*);
using FindNext_t = void(__fastcall*)(void*, bool);
using HandleKeyEvent_t = bool(__fastcall*)(void*, void*, const void*);
using EndFindSession_t = void(__fastcall*)(void*, int, int);
inline constinit Find_t o_find = nullptr;
inline constinit FindNext_t o_find_next = nullptr;
inline constinit HandleKeyEvent_t o_handle_key_event = nullptr;
inline constinit EndFindSession_t o_end_find_session = nullptr;
using AddOptionsView_t = void(__fastcall*)(void*, bool);
using ToggleOptions_t = void(__fastcall*)(void*, void*);
using ButtonToggled_t = void(__fastcall*)(void*);
inline constinit AddOptionsView_t o_add_options_view = nullptr;
inline constinit ToggleOptions_t o_toggle_options = nullptr;
inline constinit ButtonToggled_t o_button_toggled = nullptr;
using ActiveContentsChanged_t = void(__fastcall*)(void*);
inline constinit ActiveContentsChanged_t o_active_contents_changed = nullptr;

// The text changed. For our queries Edge's own marks and count are cleared first
// by asking it to find nothing; a question waits for Enter.
inline void __fastcall h_find(void* findBarView, const edge::U16View* typed) {
  if (!g_isEnabled) {
    o_find(findBarView, typed);
    return;
  }
  void* mcv = nullptr;
  Session* session = SessionOfBar(findBarView, &mcv);
  if (session) {
    ApplyBarWidth(*session, OptionOf(findBarView) == find_text::Option::kAsk);
  }
  // A typed (?i) changes the flags the closing slash shows.
  SyncDecorations(findBarView);
  const find_text::Query query = QueryOf(findBarView, TextOf(*typed));
  const bool isOurs =
      (session != nullptr) && query.mode != find_text::Mode::kText;
  if (!isOurs) {
    if (session) {
      Reset(mcv, *session);
    }
    o_find(findBarView, typed);
    return;
  }
  const edge::U16View nothing{u"", 0};
  o_find(findBarView, &nothing);
  Reset(mcv, *session);
  if (query.mode == find_text::Mode::kRegExp) {
    RunSearch(mcv, *session, query);
  } else {
    ShowStatus(findBarView, HintStatus());
  }
}

// Enter, Shift+Enter or an arrow button. A question is asked on the first Enter
// and stepped through afterwards. False leaves the press to Edge.
inline bool HandleNext(void* findBarView, bool isReverse) {
  if (!g_isEnabled) {
    return false;
  }
  void* mcv = nullptr;
  Session* session = SessionOfBar(findBarView, &mcv);
  const std::string typed = TextOf(edge::FindBarText(findBarView));
  const find_text::Query query = QueryOf(findBarView, typed);
  const bool isOurs =
      (session != nullptr) && query.mode != find_text::Mode::kText;
  if (!isOurs) {
    return false;
  }
  const find_text::Step step =
      isReverse ? find_text::Step::kPrevious : find_text::Step::kNext;
  const bool isOtherTab = (session->target != nullptr) &&
                          session->target != tab_list::ActiveContents(mcv);
  if (isOtherTab) {
    Reset(mcv, *session);
  }
  if (query.mode == find_text::Mode::kRegExp) {
    const bool isSearched = session->target != nullptr;
    if (isSearched) {
      StepBy(mcv, *session, step);
    } else {
      RunSearch(mcv, *session, query);
    }
    return true;
  }
  const bool isAsking = session->ask.id != 0;
  const bool isAnswered = session->askedText == typed;
  const bool isQuestionEmpty = query.pattern.empty();
  if (isAsking || isQuestionEmpty) {
    return true;
  }
  if (isAnswered) {
    StepBy(mcv, *session, step);
  } else {
    StartAsk(mcv, *session, typed, query.pattern);
  }
  return true;
}

inline void __fastcall h_find_next(void* findBarView, bool isReverse) {
  if (!HandleNext(findBarView, isReverse)) {
    o_find_next(findBarView, isReverse);
  }
}

// Edge turns Enter in the text field into a search of its own without going
// through FindNext.
inline bool __fastcall h_handle_key_event(void* controller, void* textfield,
                                          const void* keyEvent) {
  void* findBarView = edge::FindBarViewOfKeyController(controller);
  if (HandleLineBreakKey(findBarView, keyEvent)) {
    return true;
  }
  const bool isEnterPressed = g_isEnabled && edge::IsEnterPressed(keyEvent);
  const bool isShiftDown = GetKeyState(VK_SHIFT) < 0;
  const bool isHandled = isEnterPressed && HandleNext(findBarView, isShiftDown);
  return isHandled || o_handle_key_event(controller, textfield, keyEvent);
}

inline void __fastcall h_add_options_view(void* findBarView, bool isArgument) {
  o_add_options_view(findBarView, isArgument);
  SyncToggles(findBarView);
}

inline void __fastcall h_toggle_options(void* findBarView, void* argument) {
  o_toggle_options(findBarView, argument);
  SyncToggles(findBarView);
}

// A pattern or a question means nothing to Match case, Match whole word or Match
// diacritics: switch off the ones that are on. Edge's own callback follows the move, so the
// find parameters it keeps for them stay in step with what the bar shows.
inline void SwitchOffEdgeToggles(void* findBarView) {
  const int edgeIds[] = {edge155::off::FINDTOGGLE_id_match_case,
                         edge155::off::FINDTOGGLE_id_whole_word,
                         edge155::off::FINDTOGGLE_id_diacritics};
  for (const int toggleId : edgeIds) {
    void* toggle = edge::FindToggle(findBarView, toggleId);
    const bool isOn = (toggle != nullptr) && edge::FindToggleIsOn(toggle);
    if (isOn) {
      edge::FindToggleSetOn(toggle, false);
      o_button_toggled(toggle);
    }
  }
}

// One of the switches was clicked; Edge's three go on as Edge has them.
inline void __fastcall h_button_toggled(void* toggle) {
  const int toggleId = edge::FindToggleId(toggle);
  const bool isOurs = g_isEnabled && IsOurToggleId(toggleId);
  void* findBarView = isOurs ? edge::FindBarViewOfToggle(toggle) : nullptr;
  if (!findBarView) {
    o_button_toggled(toggle);
    return;
  }
  // The switch has already moved by the time its callback runs.
  const bool isTurningOn = edge::FindToggleIsOn(toggle);
  const find_flags::FlagInfo* flag = FlagOfToggleId(toggleId);
  const bool isFlag = flag != nullptr;
  const int otherId = toggleId == edge155::off::FINDTOGGLE_id_regexp
                          ? edge155::off::FINDTOGGLE_id_ask
                          : edge155::off::FINDTOGGLE_id_regexp;
  void* other = edge::FindToggle(findBarView, otherId);
  if (isTurningOn && other && !isFlag) {
    edge::FindToggleSetOn(other, false);
  }
  find_flags::Flag excluded = find_flags::Flag::kIndices;
  const bool isExcludingOther =
      isFlag && isTurningOn && find_flags::HasExclusive(flag->flag, &excluded);
  if (isExcludingOther) {
    void* excludedToggle =
        edge::FindToggle(findBarView, ToggleIdOfFlag(excluded));
    if (excludedToggle) {
      edge::FindToggleSetOn(excludedToggle, false);
    }
  }
  const bool isTurningModeOn = isTurningOn && !isFlag;
  if (isTurningModeOn) {
    SwitchOffEdgeToggles(findBarView);
  }
  SyncToggles(findBarView);
  const edge::U16View typed = edge::FindBarText(findBarView);
  h_find(findBarView, &typed);
}

// A switch is set again from the tab's find options, as the bar is shown again
// or the tab changes. Edge keeps no state for our ids and so turned them off
// here, with no click callback: RegExp was off on reopen while its flags and the
// slashes still showed. Ours keep the state that lives in the switch.
inline void __fastcall h_active_contents_changed(void* toggle) {
  const int toggleId = edge::FindToggleId(toggle);
  const bool isOurs = g_isEnabled && IsOurToggleId(toggleId);
  if (!isOurs) {
    o_active_contents_changed(toggle);
  }
  void* findBarView = g_isEnabled ? edge::FindBarViewOfToggle(toggle) : nullptr;
  if (findBarView) {
    SyncToggles(findBarView);
  }
}

inline void __fastcall h_end_find_session(void* controller, int action,
                                          int isKeepingSelection) {
  o_end_find_session(controller, action, isKeepingSelection);
  std::vector<void*> windows;
  for (const auto& [mcv, session] : Sessions()) {
    windows.push_back(mcv);
  }
  for (void* mcv : windows) {
    Close(mcv);
  }
}

}  // namespace edgeplus::find
