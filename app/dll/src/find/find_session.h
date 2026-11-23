// find_session.h — the enhanced find's state, one session per split view
// (MultiContentsView): its bar, its search and its question, kept while the
// bar's window lives.
#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include "assistant/ask_text.h"
#include "edge/api/browser.h"
#include "find/find_text.h"

namespace edgeplus::find {

inline constinit bool g_isEnabled = false;

struct AskRun {
  unsigned long long id =
      0;  // 0 = none; a late answer to an older one is dropped
  ask_text::Plan plan;
  size_t nextChunk = 0;
  std::vector<int> highlights;
  std::string note;  // the assistant's sentence on what it found
  bool isPageCut = false;
  std::string failure;   // why a later prompt failed, after earlier ones worked
  std::string question;  // without the leading mark
};

// One per MultiContentsView. `findBarView` is the FindBarView seen the last time
// one of its functions ran; every callback checks it is still the same one.
struct Session {
  void* findBarView = nullptr;
  HWND window = nullptr;
  void* target = nullptr;  // the tab holding the highlights
  unsigned long long searchSeq = 0;
  int count = 0;
  int current = find_text::kNoMatch;
  bool isCapped = false;
  int stockBarWidth = 0;
  int widenedBarWidth = 0;  // 0 = not widened
  bool isBusy = false;      // the label shows the spinner
  std::string askedText;    // the box text the last question was asked for
  AskRun ask;
};

inline std::map<void*, Session>& Sessions() {
  static std::map<void*, Session> sessions;
  return sessions;
}

// A closed window destroyed its views.
inline bool IsLive(void* mcv, const Session& session) {
  return (IsWindow(session.window) != 0) &&
         edge::McvForWindow(session.window) == mcv;
}

inline void DropStale() {
  for (auto entry = Sessions().begin(); entry != Sessions().end();) {
    if (IsLive(entry->first, entry->second)) {
      ++entry;
    } else {
      entry = Sessions().erase(entry);
    }
  }
}

inline Session* SessionFor(void* mcv) {
  DropStale();
  const auto entry = Sessions().find(mcv);
  return entry == Sessions().end() ? nullptr : &entry->second;
}

// A callback can arrive after the bar was closed.
inline Session* LiveSessionOf(void* mcv, void* view) {
  Session* session = SessionFor(mcv);
  const bool isSameBar = (session != nullptr) && session->findBarView == view;
  return isSameBar ? session : nullptr;
}

}  // namespace edgeplus::find
