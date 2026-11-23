// find_toggles.h — our switches on the bar, added and kept in step with the
// options: their titles, which are on, and Edge's own greyed out while one
// of ours is on.
#pragma once
#include <string>
#include "assistant/assistant.h"
#include "edge/api/find_bar_toggles.h"
#include "edge/api/views.h"
#include "edge/offsets.h"
#include "find/find_bar_text.h"
#include "find/find_decorations.h"
#include "find/find_flags.h"
#include "find/find_options.h"
#include "find/find_session.h"
#include "find/find_text.h"

namespace edgeplus::find {

inline void* EnsureToggle(void* findBarView, int toggleId, bool* isAdded) {
  void* toggle = edge::FindToggle(findBarView, toggleId);
  if (!toggle && g_isEnabled) {
    toggle = edge::AddFindToggle(findBarView, toggleId);
    *isAdded = *isAdded || toggle != nullptr;
  }
  return toggle;
}

// Edge's own three switches are greyed out while either of ours is on, since
// they mean nothing to a RegExp or a question.
inline void SyncTogglesNow(void* findBarView) {
  bool isAdded = false;
  void* regExp =
      EnsureToggle(findBarView, edge155::off::FINDTOGGLE_id_regexp, &isAdded);
  // Built right after RegExp so they sit under it.
  const bool isRegExpOn = (regExp != nullptr) && edge::FindToggleIsOn(regExp);
  const bool isShowingFlags = g_isEnabled && isRegExpOn;
  bool isFlagVisibilityChanged = false;
  for (const find_flags::FlagInfo& info : find_flags::kFlags) {
    const int flagToggleId = ToggleIdOfFlag(info.flag);
    const bool isNewToggle =
        edge::FindToggle(findBarView, flagToggleId) == nullptr;
    void* flagToggle = EnsureToggle(findBarView, flagToggleId, &isAdded);
    if (!flagToggle) {
      continue;
    }
    const bool isStartingOn = isNewToggle && find_flags::IsDefaultOn(info.flag);
    if (isStartingOn) {
      edge::FindToggleSetOn(flagToggle, true);
    }
    const std::u16string title = Utf16FromUtf8(find_flags::SwitchTitle(info));
    edge::FindToggleSetTitle(flagToggle, Utf16Of(title));
    isFlagVisibilityChanged =
        isFlagVisibilityChanged ||
        edge::ViewGetVisible(flagToggle) != isShowingFlags;
    edge::SetViewVisible(flagToggle, isShowingFlags);
  }
  void* ask =
      EnsureToggle(findBarView, edge155::off::FINDTOGGLE_id_ask, &isAdded);
  if (regExp) {
    const std::u16string title = Utf16FromUtf8(find_text::kRegExpName);
    edge::FindToggleSetTitle(regExp, Utf16Of(title));
    edge::SetViewVisible(regExp, g_isEnabled);
  }
  if (ask) {
    const std::u16string title =
        Utf16FromUtf8(find_text::AskTitle(assistant::Name()));
    edge::FindToggleSetTitle(ask, Utf16Of(title));
    edge::SetViewVisible(ask, g_isEnabled);
  }
  // RegExp and Ask each grey out every other switch while on.
  const find_text::Option option = OptionOf(findBarView);
  if (regExp) {
    edge::FindToggleSetEnabled(regExp, option != find_text::Option::kAsk);
  }
  if (ask) {
    edge::FindToggleSetEnabled(ask, option != find_text::Option::kRegExp);
  }
  const bool isEdgeLocked = option != find_text::Option::kNone;
  const int edgeIds[] = {edge155::off::FINDTOGGLE_id_match_case,
                         edge155::off::FINDTOGGLE_id_whole_word,
                         edge155::off::FINDTOGGLE_id_diacritics};
  for (const int toggleId : edgeIds) {
    void* toggle = edge::FindToggle(findBarView, toggleId);
    if (toggle) {
      edge::FindToggleSetEnabled(toggle, !isEdgeLocked);
    }
  }
  if (isAdded || isFlagVisibilityChanged) {
    edge::FindBarRelayoutOptions(findBarView);
  }
  SyncDecorations(findBarView);
}

// Building a switch makes Edge set its state (h_active_contents_changed), which
// syncs again; that inner call is dropped, the outer one finishes the job.
inline constinit bool g_isSyncingToggles = false;
inline void SyncToggles(void* findBarView) {
  if (g_isSyncingToggles) {
    return;
  }
  g_isSyncingToggles = true;
  SyncTogglesNow(findBarView);
  g_isSyncingToggles = false;
}

}  // namespace edgeplus::find
