// find_ask.h — a question for the browser assistant: the page's paragraphs
// read and sent to it in chunks, its answer turned into highlights, and
// the question ended, failed or cancelled.
#pragma once
#include <string>
#include <vector>
#include "assistant/ask_text.h"
#include "assistant/assistant.h"
#include "assistant/assistant_chat.h"
#include "edge/api/browser.h"
#include "edge/api/find_bar.h"
#include "edge/api/scripts.h"
#include "edge/api/views.h"
#include "find/find_poll.h"
#include "find/find_scripts.h"
#include "find/find_search.h"
#include "find/find_session.h"
#include "find/find_status.h"
#include "find/find_text.h"
#include "find/tab_list.h"

namespace edgeplus::find {

inline void EndAsk(Session& session) {
  session.ask = AskRun{};
  assistant_chat::ReleasePanel(edge::McvForWindow(session.window));
  // The timer stops with the ask, so a spinner left up would freeze.
  const bool isSpinnerLeft = session.isBusy && (session.findBarView != nullptr);
  if (isSpinnerLeft) {
    ShowStatus(session.findBarView, Status{});
  }
}

inline void FailAsk(Session& session, const std::string& why) {
  EndAsk(session);
  session.askedText.clear();
  ShowStatus(session.findBarView, FailedStatus(why));
}

inline void CancelAsk(void* mcv, Session& session) {
  const bool isAsking = session.ask.id != 0;
  if (isAsking) {
    assistant_chat::Cancel(mcv);
    EndAsk(session);
  }
}

inline void OnHighlighted(void* mcv, void* view, unsigned long long askId,
                          const edge::ScriptResult& result) {
  Session* session = LiveSessionOf(mcv, view);
  const bool isCurrentAsk = (session != nullptr) && session->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  const find_text::SearchResult parsed =
      result.isString ? find_text::ParseSearchResult(result.text)
                      : find_text::SearchResult{};
  std::string note =
      session->ask.failure.empty() ? session->ask.note : session->ask.failure;
  const bool isCut = session->ask.isPageCut || session->ask.plan.isCut;
  EndAsk(*session);
  if (!parsed.isOk || parsed.count == 0) {
    FailAsk(*session,
            parsed.error.empty() ? kPageChangedMessage : parsed.error);
    return;
  }
  session->count = parsed.count;
  session->current = parsed.current;
  session->isCapped = false;
  ShowCount(*session);
  if (note.empty()) {
    note = std::to_string(parsed.count) +
           (parsed.count == 1 ? " passage marked." : " passages marked.");
  }
  if (isCut) {
    note += kLongPageMessage;
  }
  edge::ViewSetTooltip(edge::FindBarLabel(view), note.c_str());
}

inline void FinishAsk(void* mcv, Session& session) {
  const bool isNothing = session.ask.highlights.empty();
  if (isNothing) {
    std::string why = std::string(assistant::Name()) +
                      " found no paragraph that answers that.";
    if (!session.ask.failure.empty()) {
      why = session.ask.failure;
    } else if (!session.ask.note.empty()) {
      why = session.ask.note;
    }
    EndAsk(session);
    ShowStatus(session.findBarView, NoParagraphStatus(why));
    return;
  }
  const bool isTargetActive = IsTabAlive(mcv, session.target) &&
                              tab_list::ActiveContents(mcv) == session.target;
  if (!isTargetActive) {
    FailAsk(session, kTabChangedMessage);
    return;
  }
  ask_text::SortAscending(&session.ask.highlights);
  const unsigned long long askId = session.ask.id;
  void* view = session.findBarView;
  const bool isRunning = edge::RunScript(
      session.target,
      find_scripts::HighlightParagraphsScript(session.ask.highlights),
      [mcv, view, askId](const edge::ScriptResult& result) {
        OnHighlighted(mcv, view, askId, result);
      });
  if (!isRunning) {
    FailAsk(session, kPageChangedMessage);
  }
}

inline void SendNextChunk(void* mcv, Session& session);

inline void OnAssistantReply(void* mcv, void* view, unsigned long long askId,
                             const ask_text::Reply& reply) {
  Session* session = LiveSessionOf(mcv, view);
  const bool isCurrentAsk = (session != nullptr) && session->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  AskRun& ask = session->ask;
  if (!reply.isOk) {
    const bool hasEarlierAnswers = !ask.highlights.empty();
    if (hasEarlierAnswers) {
      ask.failure =
          std::string(assistant::Name()) + " stopped early: " + reply.error;
      FinishAsk(mcv, *session);
    } else {
      FailAsk(*session, reply.error);
    }
    return;
  }
  const ask_text::Answer answer =
      ask_text::ParseAnswer(reply.text, ask.plan.chunks[ask.nextChunk].indices);
  ask_text::Accumulate(&ask.highlights, answer);
  if (ask.note.empty() && !answer.isNone) {
    ask.note = answer.note;
  }
  ask.nextChunk++;
  const bool hasMoreChunks = ask.nextChunk < ask.plan.chunks.size();
  if (hasMoreChunks) {
    SendNextChunk(mcv, *session);
  } else {
    FinishAsk(mcv, *session);
  }
}

inline void OnAssistantNotice(void* mcv, void* view, unsigned long long askId,
                              assistant_chat::Notice notice) {
  const Session* session = LiveSessionOf(mcv, view);
  const bool isCurrentAsk = (session != nullptr) && session->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  const bool isNeedingUser = notice == assistant_chat::Notice::kNeedsUser;
  ShowStatus(view, isNeedingUser ? NeedsUserStatus() : WaitingStatus());
}

inline void SendNextChunk(void* mcv, Session& session) {
  AskRun& ask = session.ask;
  ShowStatus(session.findBarView,
             AskingStatus(ask.nextChunk + 1, ask.plan.chunks.size()));
  const unsigned long long askId = ask.id;
  void* view = session.findBarView;
  const bool isAsked = assistant_chat::Ask(
      session.window, mcv, ask.plan.chunks[ask.nextChunk].prompt,
      [mcv, view, askId](const ask_text::Reply& reply) {
        OnAssistantReply(mcv, view, askId, reply);
      },
      [mcv, view, askId](assistant_chat::Notice notice) {
        OnAssistantNotice(mcv, view, askId, notice);
      });
  if (!isAsked) {
    FailAsk(session, std::string(assistant::Name()) +
                         " could not be opened in this window.");
  }
}

inline void OnParagraphsRead(void* mcv, void* view, unsigned long long askId,
                             const edge::ScriptResult& result) {
  Session* session = LiveSessionOf(mcv, view);
  const bool isCurrentAsk = (session != nullptr) && session->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  const ask_text::PageParagraphs page =
      result.isString ? ask_text::ParsePageParagraphs(result.text)
                      : ask_text::PageParagraphs{};
  if (!page.isOk) {
    FailAsk(*session, page.error.empty() ? kNoAnswerMessage : page.error);
    return;
  }
  const std::vector<ask_text::Sentence> sentences =
      ask_text::Prepare(page.texts, page.paragraphs);
  session->ask.plan = ask_text::PlanQuestion(session->ask.question, sentences);
  session->ask.isPageCut = page.isCut;
  if (session->ask.plan.chunks.empty()) {
    FailAsk(*session, kNoTextMessage);
    return;
  }
  SendNextChunk(mcv, *session);
}

inline void StartAsk(void* mcv, Session& session, const std::string& typedText,
                     const std::string& question) {
  static unsigned long long nextAskId = 1;
  CancelAsk(mcv, session);
  ClearTarget(mcv, session);
  void* contents = tab_list::ActiveContents(mcv);
  if (!CanSearch(contents)) {
    ShowStatus(session.findBarView, FailedStatus(kCannotSearchMessage));
    return;
  }
  session.askedText = typedText;
  session.target = contents;
  session.ask = AskRun{};
  session.ask.id = nextAskId++;
  session.ask.question = question;
  ShowStatus(session.findBarView, ReadingStatus());
  const unsigned long long askId = session.ask.id;
  void* view = session.findBarView;
  const bool isRunning =
      edge::RunScript(contents, find_scripts::ReadParagraphsScript(),
                      [mcv, view, askId](const edge::ScriptResult& result) {
                        OnParagraphsRead(mcv, view, askId, result);
                      });
  if (!isRunning) {
    FailAsk(session, kCannotSearchMessage);
    return;
  }
  StartTimer();
}

}  // namespace edgeplus::find
