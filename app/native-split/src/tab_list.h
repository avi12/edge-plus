// tab_list.h - a browser window's tabs by id, as the enhanced find reads them.
#pragma once
#include <string>
#include "edge.h"

namespace fixedge::tab_list {

inline std::string TabIdOf(void* contents) {
  return std::to_string(reinterpret_cast<uintptr_t>(contents));
}

// Null if it was closed.
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

}  // namespace fixedge::tab_list
