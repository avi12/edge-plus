// find_search.h — a RegExp searched by us in the tab: running it, stepping
// through its matches, and clearing the highlights from the tab it ran in.
#pragma once
#include "core/log.h"
#include "edge/api/scripts.h"
#include "edge/api/web_contents.h"
#include "find/find_scripts.h"
#include "find/find_session.h"
#include "find/find_status.h"
#include "find/find_text.h"
#include "find/tab_list.h"
#include "find/web_text.h"

namespace edgeplus::find {

// A tab that was closed must never be run a script.
inline bool IsTabAlive(void* mcv, void* contents) {
  return (contents != nullptr) &&
         (tab_list::FindTab(mcv, tab_list::TabIdOf(contents)) != nullptr);
}

inline bool CanSearch(void* contents) {
  return (contents != nullptr) && edge::IsPageLive(contents) &&
         find_text::IsSearchableUrl(edge::TabUrl(contents));
}

inline void ClearTarget(void* mcv, Session& session) {
  const bool isClearable =
      IsTabAlive(mcv, session.target) && edge::IsPageLive(session.target);
  if (isClearable) {
    edge::RunScript(session.target, find_scripts::ClearScript(), nullptr);
  }
  session.target = nullptr;
  session.count = 0;
  session.current = find_text::kNoMatch;
  session.isCapped = false;
  session.searchSeq++;
}

inline void OnSearched(void* mcv, void* view, unsigned long long seq,
                       const edge::ScriptResult& result) {
  Session* session = LiveSessionOf(mcv, view);
  const bool isCurrentSearch =
      (session != nullptr) && session->searchSeq == seq;
  if (!isCurrentSearch) {
    return;
  }
  const find_text::SearchResult parsed =
      result.isString ? find_text::ParseSearchResult(result.text)
                      : find_text::SearchResult{};
  if (!parsed.isOk) {
    session->count = 0;
    session->current = find_text::kNoMatch;
    ShowStatus(view, BadRegExpStatus(parsed.error.empty() ? kNoAnswerMessage
                                                          : parsed.error));
    return;
  }
  session->count = parsed.count;
  session->current = parsed.count > 0 ? parsed.current : find_text::kNoMatch;
  session->isCapped = parsed.isCapped;
  ShowCount(*session);
}

// Search the active tab for a RegExp: mark every match, jump to the first one
// in view, show the count. A pattern that cannot run says why.
inline void RunSearch(void* mcv, Session& session,
                      const find_text::Query& query) {
  const find_text::Search search = find_text::BuildSearch(query);
  const bool isNothingTyped =
      search.problem == find_text::PatternProblem::kEmpty;
  if (isNothingTyped) {
    ClearTarget(mcv, session);
    ClearLabel(session);
    return;
  }
  const bool isUnusable = search.problem != find_text::PatternProblem::kNone;
  if (isUnusable) {
    ClearTarget(mcv, session);
    ShowStatus(session.findBarView,
               BadRegExpStatus(find_text::ProblemText(search.problem)));
    return;
  }
  void* contents = tab_list::ActiveContents(mcv);
  if (!CanSearch(contents)) {
    ShowStatus(session.findBarView, FailedStatus(kCannotSearchMessage));
    return;
  }
  const bool isOtherTab =
      (session.target != nullptr) && session.target != contents;
  if (isOtherTab) {
    ClearTarget(mcv, session);
  }
  session.target = contents;
  Log("[find] RegExp search with flags \"%s\"", query.flags.c_str());
  const unsigned long long seq = ++session.searchSeq;
  void* view = session.findBarView;
  const bool isRunning =
      edge::RunScript(contents, find_scripts::SearchScript(search),
                      [mcv, view, seq](const edge::ScriptResult& result) {
                        OnSearched(mcv, view, seq, result);
                      });
  if (!isRunning) {
    ShowStatus(view, FailedStatus(kCannotSearchMessage));
  }
}

inline void OnWentTo(void* mcv, void* view, unsigned long long seq, int index,
                     const edge::ScriptResult& result) {
  Session* session = LiveSessionOf(mcv, view);
  const bool isCurrentSearch =
      (session != nullptr) && session->searchSeq == seq;
  if (!isCurrentSearch) {
    return;
  }
  const bool isMoved = result.isString && result.text == web_text::kOkTag;
  if (isMoved) {
    session->current = index;
    ShowCount(*session);
    return;
  }
  // The page no longer holds the marks: it was reloaded.
  ClearTarget(mcv, *session);
  ShowStatus(view, FailedStatus(kPageChangedMessage));
}

inline void StepBy(void* mcv, Session& session, find_text::Step step) {
  const bool hasMatches = session.count > 0 && (session.target != nullptr);
  if (!hasMatches) {
    return;
  }
  const bool isTargetActive = tab_list::ActiveContents(mcv) == session.target;
  if (!isTargetActive) {
    return;
  }
  const int next = find_text::StepMatch(session.current, session.count, step);
  const unsigned long long seq = session.searchSeq;
  void* view = session.findBarView;
  edge::RunScript(session.target, find_scripts::GoToScript(next),
                  [mcv, view, seq, next](const edge::ScriptResult& result) {
                    OnWentTo(mcv, view, seq, next, result);
                  });
}

}  // namespace edgeplus::find
