// tab_list.h - a browser window's tabs, as the sharing code reads them. One
// concern: find a tab by the id the picker or the glic host was given, name the
// active one, and say whether a tab can be shared (share.h and glic.h both ask).
#pragma once
#include <map>
#include <string>
#include <vector>
#include "edge.h"
#include "share_text.h"

namespace fixedge::tab_list {

inline std::string TabIdOf(void* contents) {
  return std::to_string(reinterpret_cast<uintptr_t>(contents));
}

// The tab with picker id `tabId` in `mcv`'s window, or null if it was closed.
inline void* FindTab(void* mcv, const std::string& tabId) {
  void* tsm = edge::McvTabStripModel(mcv);
  const int count = tsm ? edge::TabCount(tsm) : 0;
  for (int i = 0; i < count; i++) {
    void* contents = edge::TabWebContentsAt(tsm, i);
    if (contents && TabIdOf(contents) == tabId) {
      return contents;
    }
  }
  return nullptr;
}

inline void* ActiveContents(void* mcv) {
  void* tsm = edge::McvTabStripModel(mcv);
  void* tab = tsm ? edge::ActiveTab(tsm) : nullptr;
  return tab ? edge::TabContents(tab) : nullptr;
}

inline share_text::TabState StateOf(void* contents) {
  if (!share_text::IsShareableUrl(edge::TabUrl(contents))) {
    return share_text::TabState::kBlocked;
  }
  return edge::IsPageLive(contents) ? share_text::TabState::kReady
                                    : share_text::TabState::kAsleep;
}

inline std::vector<share_text::TabEntry> ListTabs(void* mcv) {
  std::vector<share_text::TabEntry> entries;
  void* tsm = edge::McvTabStripModel(mcv);
  const int count = tsm ? edge::TabCount(tsm) : 0;
  const void* active = ActiveContents(mcv);
  for (int i = 0; i < count; i++) {
    void* contents = edge::TabWebContentsAt(tsm, i);
    if (!contents) {
      continue;
    }
    share_text::TabEntry entry;
    entry.id = TabIdOf(contents);
    entry.url = edge::TabUrl(contents);
    entry.title = edge::TabTitle(contents);
    if (entry.title.empty()) {
      entry.title = entry.url;
    }
    entry.isCurrent = contents == active;
    entry.state = StateOf(contents);
    entries.push_back(entry);
  }
  return entries;
}

// The small integer the glic host names a tab by (Chrome's tab ids are int32):
// assigned the first time a tab is seen and kept for its life.
inline std::map<void*, int>& IntIds() {
  static std::map<void*, int> ids;
  return ids;
}
inline int IntIdOf(void* contents) {
  static int nextId = 1;
  const auto found = IntIds().find(contents);
  if (found != IntIds().end()) {
    return found->second;
  }
  IntIds()[contents] = nextId;
  return nextId++;
}

// The tab named by glic host id `tabId` in `mcv`'s window, or null.
inline void* FindByIntId(void* mcv, int tabId) {
  void* tsm = edge::McvTabStripModel(mcv);
  const int count = tsm ? edge::TabCount(tsm) : 0;
  for (int i = 0; i < count; i++) {
    void* contents = edge::TabWebContentsAt(tsm, i);
    const auto found = IntIds().find(contents);
    if (found != IntIds().end() && found->second == tabId) {
      return contents;
    }
  }
  return nullptr;
}

}  // namespace fixedge::tab_list
