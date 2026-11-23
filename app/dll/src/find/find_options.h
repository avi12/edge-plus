// find_options.h — the options our switches on the bar set (RegExp, Ask and
// the RegExp flags), the toggle id of each, and the query they make of the
// box's text.
#pragma once
#include <string>
#include "edge/api/find_bar_toggles.h"
#include "edge/offsets.h"
#include "find/find_flags.h"
#include "find/find_session.h"
#include "find/find_text.h"

namespace edgeplus::find {

// The switches are the bar's own views, so their state is per window.
inline find_text::Option OptionOf(void* findBarView) {
  if (!g_isEnabled) {
    return find_text::Option::kNone;
  }
  void* regExp =
      edge::FindToggle(findBarView, edge155::off::FINDTOGGLE_id_regexp);
  void* ask = edge::FindToggle(findBarView, edge155::off::FINDTOGGLE_id_ask);
  if (regExp && edge::FindToggleIsOn(regExp)) {
    return find_text::Option::kRegExp;
  }
  if (ask && edge::FindToggleIsOn(ask)) {
    return find_text::Option::kAsk;
  }
  return find_text::Option::kNone;
}

inline int ToggleIdOfFlag(find_flags::Flag flag) {
  switch (flag) {
    case find_flags::Flag::kIndices:
      return edge155::off::FINDTOGGLE_id_flag_indices;
    case find_flags::Flag::kIgnoreCase:
      return edge155::off::FINDTOGGLE_id_flag_ignore_case;
    case find_flags::Flag::kMultiline:
      return edge155::off::FINDTOGGLE_id_flag_multiline;
    case find_flags::Flag::kDotAll:
      return edge155::off::FINDTOGGLE_id_flag_dot_all;
    case find_flags::Flag::kUnicode:
      return edge155::off::FINDTOGGLE_id_flag_unicode;
    case find_flags::Flag::kUnicodeSets:
      return edge155::off::FINDTOGGLE_id_flag_unicode_sets;
    case find_flags::Flag::kSticky:
      return edge155::off::FINDTOGGLE_id_flag_sticky;
  }
  return edge155::off::FINDTOGGLE_id_flag_indices;
}

inline const find_flags::FlagInfo* FlagOfToggleId(int toggleId) {
  for (const find_flags::FlagInfo& info : find_flags::kFlags) {
    if (ToggleIdOfFlag(info.flag) == toggleId) {
      return &info;
    }
  }
  return nullptr;
}

inline bool IsFlagToggleId(int toggleId) {
  return FlagOfToggleId(toggleId) != nullptr;
}

inline bool IsOurToggleId(int toggleId) {
  return toggleId == edge155::off::FINDTOGGLE_id_regexp ||
         toggleId == edge155::off::FINDTOGGLE_id_ask ||
         IsFlagToggleId(toggleId);
}

inline find_flags::FlagSet FlagsOn(void* findBarView) {
  find_flags::FlagSet set;
  for (const find_flags::FlagInfo& info : find_flags::kFlags) {
    void* toggle = edge::FindToggle(findBarView, ToggleIdOfFlag(info.flag));
    if (toggle && edge::FindToggleIsOn(toggle)) {
      find_flags::SetOn(&set, info.flag, true);
    }
  }
  return set;
}

// The flag switches count only while RegExp is the chosen switch: they are
// hidden otherwise, so a stale one must never reach a shortcut such as
// /pattern/.
inline find_text::Query QueryOf(void* findBarView, const std::string& typed) {
  const find_text::Option option = OptionOf(findBarView);
  find_text::Query query = find_text::ParseQuery(typed, option);
  const bool isUsingFlagSwitches = option == find_text::Option::kRegExp &&
                                   query.mode == find_text::Mode::kRegExp;
  if (isUsingFlagSwitches) {
    query.flags = find_flags::Merge(query.flags, FlagsOn(findBarView));
    query.isFlagsFinal = true;
  }
  return query;
}

}  // namespace edgeplus::find
