// find.h — the enhanced find bar. One concern: when the user presses Ctrl+F (the
// panel's switch for it is on), show a bar of our own over the top right of the
// page instead of Edge's, and run what the user types in the page: a text or
// regular-expression search with every match marked and a count, or a question
// for Copilot whose answer is the paragraphs to mark. The bar is a page of our
// own in a WebView (find_page.h); this file polls it for what the user did, runs
// the scripts of find_scripts.h in the tab being searched, and keeps the bar
// glued to the page. The pure parts are find_text.h and ask_text.h; talking to
// Copilot is copilot_chat.h's.
//
// Like the share strip, the bar cannot call us: a UI-thread timer asks it for
// its events a few times a second while it is open (share.h does the same for
// the Gemini panel). Nothing runs while it is closed.
#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "ask_text.h"
#include "channel.h"
#include "assistant.h"
#include "assistant_chat.h"
#include "edge.h"
#include "find_layout.h"
#include "find_page.h"
#include "find_scripts.h"
#include "find_text.h"
#include "reserve.h"
#include "share_text.h"
#include "tab_list.h"

namespace fixedge::find {

using share_text::StatusKind;

// Whether the find bar is switched on; the panel pushes it (config.h).
inline bool g_isEnabled = false;

inline bool IsEnabled() {
  return g_isEnabled;
}

inline constexpr UINT_PTR kTimerId = 0xF1EC;
inline constexpr UINT kPollIntervalMs = 100;

// What the bar says. The one place its words are written.
inline constexpr char kCannotSearchMessage[] =
    "This page cannot be searched here.";
inline constexpr char kNoAnswerMessage[] = "The page did not answer.";
inline constexpr char kAskPromptMessage[] =
    "Press Enter to ask Copilot about this page.";
inline constexpr char kReadingMessage[] = "Reading the page...";
inline constexpr char kNoTextMessage[] = "This page has no text to ask about.";
inline constexpr char kNoCopilotMessage[] =
    "Copilot could not be opened in this window.";
inline constexpr char kTabChangedMessage[] =
    "The tab changed. Ask again for this page.";
inline constexpr char kPageChangedMessage[] = "The page changed. Ask again.";
inline constexpr char kSignInMessage[] =
    "Copilot needs you: see its page below.";
inline constexpr char kWaitingMessage[] = "Waiting for Copilot...";
inline constexpr char kLongPageMessage[] =
    " Only the first part of this long page was searched.";

// Where the bar is in showing the user's selection when it opens: the box is
// filled once the bar's script is up and the page has said what is selected.
enum class OpenStep {
  kNone,
  kReadingSelection,
  kReady,
};

// One question for Copilot in flight.
struct AskRun {
  unsigned long long id =
      0;  // 0 = none; a late answer to an older one is dropped
  ask_text::Plan plan;
  size_t nextChunk = 0;
  std::vector<int> highlights;
  std::string note;  // Copilot's sentence on what it found
  bool isPageCut = false;
  std::string failure;  // why a later prompt failed, after earlier ones worked
};

// One bar per MultiContentsView (one per browser window).
struct Bar {
  void* view = nullptr;
  void* parent = nullptr;
  HWND window = nullptr;
  bool isOpen = false;
  bool isInstalled = false;  // the bar page's script is running
  bool isPolling = false;    // a poll or install is in flight
  OpenStep openStep = OpenStep::kNone;
  std::string selection;
  find_text::Options options;
  std::string text;        // the box, as last reported
  void* target = nullptr;  // the tab holding the highlights
  unsigned long long searchSeq = 0;
  int count = 0;
  int current = find_text::kNoMatch;
  bool isCapped = false;
  AskRun ask;
};

inline std::map<void*, Bar>& Bars() {
  static std::map<void*, Bar> bars;
  return bars;
}

// Whether `bar` still belongs to `mcv`: its window exists and still shows that
// MultiContentsView (a closed window destroyed its views, ours included).
inline bool IsLive(void* mcv, const Bar& bar) {
  return IsWindow(bar.window) && edge::McvForWindow(bar.window) == mcv;
}

inline void DropStale() {
  for (auto entry = Bars().begin(); entry != Bars().end();) {
    if (IsLive(entry->first, entry->second)) {
      ++entry;
    } else {
      entry = Bars().erase(entry);
    }
  }
}

inline Bar* BarFor(void* mcv) {
  DropStale();
  const auto entry = Bars().find(mcv);
  return entry == Bars().end() ? nullptr : &entry->second;
}

// The open bar of `mcv` if it is still the one whose view is `view`: every
// callback checks this, since a callback can arrive after the bar was closed.
inline Bar* OpenBarOf(void* mcv, void* view) {
  Bar* bar = BarFor(mcv);
  const bool isSameBar = bar && bar->view == view && bar->isOpen;
  return isSameBar ? bar : nullptr;
}

// --- talking to the bar page ------------------------------------------------------------

inline void CallBar(const Bar& bar, const std::string& statement) {
  void* contents = edge::WebViewContents(bar.view);
  if (contents) {
    edge::RunScript(contents, "window.__ff && " + statement + "; 0", nullptr);
  }
}

inline void SetCount(const Bar& bar, const std::string& label) {
  CallBar(bar,
          "window.__ff.setCount(" + share_text::JsStringLiteral(label) + ")");
}

inline void SetStatus(const Bar& bar, StatusKind kind,
                      const std::string& text) {
  CallBar(bar,
          "window.__ff.setStatus(" +
              share_text::JsStringLiteral(share_text::StatusKindName(kind)) +
              "," + share_text::JsStringLiteral(text) + ")");
}

inline void SetBusy(const Bar& bar, bool isBusy) {
  CallBar(bar, std::string("window.__ff.setBusy(") +
                   (isBusy ? "true" : "false") + ")");
}

// The count the bar shows for the matches as they stand.
inline void ShowCount(const Bar& bar) {
  SetCount(bar, find_text::MatchLabel(bar.current, bar.count, bar.isCapped));
}

// --- the page being searched --------------------------------------------------------------

// Whether `contents` is still a tab of `mcv`'s window: a tab that was closed
// must never be run a script.
inline bool IsTabAlive(void* mcv, void* contents) {
  return contents && tab_list::FindTab(mcv, tab_list::TabIdOf(contents));
}

inline bool CanSearch(void* contents) {
  return contents && edge::IsPageLive(contents) &&
         find_text::IsSearchableUrl(edge::TabUrl(contents));
}

// Take the highlights out of the tab that holds them (if it is still there)
// and forget the count.
inline void ClearTarget(void* mcv, Bar& bar) {
  const bool isClearable =
      IsTabAlive(mcv, bar.target) && edge::IsPageLive(bar.target);
  if (isClearable) {
    edge::RunScript(bar.target, find_scripts::kClearScript, nullptr);
  }
  bar.target = nullptr;
  bar.count = 0;
  bar.current = find_text::kNoMatch;
  bar.isCapped = false;
  bar.searchSeq++;
}

inline void RunSearch(void* mcv, Bar& bar);

inline void OnSearched(void* mcv, void* view, unsigned long long seq,
                       const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  const bool isCurrentSearch = bar && bar->searchSeq == seq;
  if (!isCurrentSearch) {
    return;
  }
  const find_text::SearchResult parsed =
      result.isString ? find_text::ParseSearchResult(result.text)
                      : find_text::SearchResult{};
  if (!parsed.isOk) {
    bar->count = 0;
    bar->current = find_text::kNoMatch;
    SetCount(*bar, "");
    SetStatus(*bar, StatusKind::kError,
              parsed.error.empty() ? kNoAnswerMessage : parsed.error);
    return;
  }
  bar->count = parsed.count;
  bar->current = parsed.count > 0 ? parsed.current : find_text::kNoMatch;
  bar->isCapped = parsed.isCapped;
  SetStatus(*bar, StatusKind::kInfo, "");
  if (parsed.count > 0) {
    ShowCount(*bar);
  } else {
    SetCount(*bar, find_text::MatchLabel(find_text::kNoMatch, 0, false));
  }
}

// Search the active tab for what the box holds: mark every match, jump to the
// first one in view, show the count. A box that is empty clears; a pattern that
// cannot run says why.
inline void RunSearch(void* mcv, Bar& bar) {
  const find_text::Search search =
      find_text::BuildSearch(bar.text, bar.options);
  const bool isEmptyBox = search.problem == find_text::PatternProblem::kEmpty;
  if (isEmptyBox) {
    ClearTarget(mcv, bar);
    SetCount(bar, "");
    SetStatus(bar, StatusKind::kInfo, "");
    return;
  }
  const bool isUnusable = search.problem != find_text::PatternProblem::kNone;
  if (isUnusable) {
    ClearTarget(mcv, bar);
    SetCount(bar, "");
    SetStatus(bar, StatusKind::kError, find_text::ProblemText(search.problem));
    return;
  }
  void* contents = tab_list::ActiveContents(mcv);
  if (!CanSearch(contents)) {
    SetStatus(bar, StatusKind::kError, kCannotSearchMessage);
    return;
  }
  const bool isOtherTab = bar.target && bar.target != contents;
  if (isOtherTab) {
    ClearTarget(mcv, bar);
  }
  bar.target = contents;
  const unsigned long long seq = ++bar.searchSeq;
  void* view = bar.view;
  const bool isRunning =
      edge::RunScript(contents, find_scripts::SearchScript(search),
                      [mcv, view, seq](const edge::ScriptResult& result) {
                        OnSearched(mcv, view, seq, result);
                      });
  if (!isRunning) {
    SetStatus(bar, StatusKind::kError, kCannotSearchMessage);
  }
}

// --- next and previous ------------------------------------------------------------------------

inline void OnWentTo(void* mcv, void* view, unsigned long long seq, int index,
                     const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  const bool isCurrentSearch = bar && bar->searchSeq == seq;
  if (!isCurrentSearch) {
    return;
  }
  const bool isMoved = result.isString && result.text == "ok";
  if (isMoved) {
    bar->current = index;
    ShowCount(*bar);
    return;
  }
  // The page no longer holds the marks (it was reloaded): mark them again, or,
  // for Copilot's answer, which cannot be remade for free, say so.
  const bool isAsking = bar->options.mode == find_text::Mode::kAsk;
  if (isAsking) {
    ClearTarget(mcv, *bar);
    SetCount(*bar, "");
    SetStatus(*bar, StatusKind::kError, kPageChangedMessage);
  } else {
    RunSearch(mcv, *bar);
  }
}

inline void StepBy(void* mcv, Bar& bar, find_text::Step step) {
  const bool hasMatches = bar.count > 0 && bar.target;
  if (!hasMatches) {
    return;
  }
  const bool isTargetActive = tab_list::ActiveContents(mcv) == bar.target;
  if (!isTargetActive) {
    return;
  }
  const int next = find_text::StepMatch(bar.current, bar.count, step);
  const unsigned long long seq = bar.searchSeq;
  void* view = bar.view;
  edge::RunScript(bar.target, find_scripts::GoToScript(next),
                  [mcv, view, seq, next](const edge::ScriptResult& result) {
                    OnWentTo(mcv, view, seq, next, result);
                  });
}

// --- Ask Copilot -------------------------------------------------------------------------------

inline void EndAsk(Bar& bar) {
  bar.ask = AskRun{};
  SetBusy(bar, false);
}

// The question is over without an answer to show.
inline void FailAsk(Bar& bar, const std::string& why) {
  EndAsk(bar);
  SetStatus(bar, StatusKind::kError, why);
}

inline void CancelAsk(void* mcv, Bar& bar) {
  const bool isAsking = bar.ask.id != 0;
  if (isAsking) {
    assistant_chat::Cancel(mcv);
    EndAsk(bar);
  }
}

inline void OnHighlighted(void* mcv, void* view, unsigned long long askId,
                          const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  const bool isCurrentAsk = bar && bar->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  const find_text::SearchResult parsed =
      result.isString ? find_text::ParseSearchResult(result.text)
                      : find_text::SearchResult{};
  // What Copilot said, or why it stopped early, and whether the page was long
  // enough that only a part of it went to Copilot.
  const std::string note =
      bar->ask.failure.empty() ? bar->ask.note : bar->ask.failure;
  const bool isCut = bar->ask.isPageCut || bar->ask.plan.isCut;
  EndAsk(*bar);
  if (!parsed.isOk || parsed.count == 0) {
    FailAsk(*bar, parsed.error.empty() ? kPageChangedMessage : parsed.error);
    return;
  }
  bar->count = parsed.count;
  bar->current = parsed.current;
  bar->isCapped = false;
  ShowCount(*bar);
  std::string text = note;
  if (text.empty()) {
    const bool isSingle = parsed.count == 1;
    text = std::to_string(parsed.count) +
           (isSingle ? " passage marked." : " passages marked.");
  }
  if (isCut) {
    text += kLongPageMessage;
  }
  SetStatus(*bar, StatusKind::kOk, text);
}

// Every prompt is answered: mark what Copilot named.
inline void FinishAsk(void* mcv, Bar& bar) {
  const bool isNothing = bar.ask.highlights.empty();
  if (isNothing) {
    std::string why = "Copilot found no paragraph that answers that.";
    if (!bar.ask.failure.empty()) {
      why = bar.ask.failure;
    } else if (!bar.ask.note.empty()) {
      why = bar.ask.note;
    }
    EndAsk(bar);
    SetCount(bar, "");
    SetStatus(bar, StatusKind::kInfo, why);
    return;
  }
  const bool isTargetActive = IsTabAlive(mcv, bar.target) &&
                              tab_list::ActiveContents(mcv) == bar.target;
  if (!isTargetActive) {
    FailAsk(bar, kTabChangedMessage);
    return;
  }
  ask_text::SortAscending(&bar.ask.highlights);
  const unsigned long long askId = bar.ask.id;
  void* view = bar.view;
  const bool isRunning = edge::RunScript(
      bar.target, find_scripts::HighlightParagraphsScript(bar.ask.highlights),
      [mcv, view, askId](const edge::ScriptResult& result) {
        OnHighlighted(mcv, view, askId, result);
      });
  if (!isRunning) {
    FailAsk(bar, kPageChangedMessage);
  }
}

inline void SendNextChunk(void* mcv, Bar& bar);

inline void OnCopilotReply(void* mcv, void* view, unsigned long long askId,
                           const ask_text::Reply& reply) {
  Bar* bar = OpenBarOf(mcv, view);
  const bool isCurrentAsk = bar && bar->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  AskRun& ask = bar->ask;
  if (!reply.isOk) {
    const bool hasEarlierAnswers = !ask.highlights.empty();
    if (hasEarlierAnswers) {
      ask.failure = "Copilot stopped early: " + reply.error;
      FinishAsk(mcv, *bar);
    } else {
      FailAsk(*bar, reply.error);
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
    SendNextChunk(mcv, *bar);
  } else {
    FinishAsk(mcv, *bar);
  }
}

inline void OnCopilotNotice(void* mcv, void* view, unsigned long long askId,
                            assistant_chat::Notice notice) {
  const Bar* bar = OpenBarOf(mcv, view);
  const bool isCurrentAsk = bar && bar->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  const bool isNeedingUser = notice == assistant_chat::Notice::kNeedsUser;
  SetStatus(*bar, isNeedingUser ? StatusKind::kError : StatusKind::kInfo,
            isNeedingUser ? kSignInMessage : kWaitingMessage);
}

inline void SendNextChunk(void* mcv, Bar& bar) {
  AskRun& ask = bar.ask;
  const std::string progress =
      ask.plan.chunks.size() > 1
          ? " (part " + std::to_string(ask.nextChunk + 1) + " of " +
                std::to_string(ask.plan.chunks.size()) + ")"
          : std::string();
  SetStatus(bar, StatusKind::kInfo, "Asking Copilot" + progress + "...");
  const unsigned long long askId = ask.id;
  void* view = bar.view;
  const bool isAsked = assistant_chat::Ask(
      bar.window, mcv, assistant::g_assistant,
      ask.plan.chunks[ask.nextChunk].prompt,
      [mcv, view, askId](const ask_text::Reply& reply) {
        OnCopilotReply(mcv, view, askId, reply);
      },
      [mcv, view, askId](assistant_chat::Notice notice) {
        OnCopilotNotice(mcv, view, askId, notice);
      });
  if (!isAsked) {
    FailAsk(bar, kNoCopilotMessage);
  }
}

inline void OnParagraphsRead(void* mcv, void* view, unsigned long long askId,
                             const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  const bool isCurrentAsk = bar && bar->ask.id == askId;
  if (!isCurrentAsk) {
    return;
  }
  const ask_text::PageParagraphs page =
      result.isString ? ask_text::ParsePageParagraphs(result.text)
                      : ask_text::PageParagraphs{};
  if (!page.isOk) {
    FailAsk(*bar, page.error.empty() ? kNoAnswerMessage : page.error);
    return;
  }
  const std::vector<ask_text::Paragraph> paragraphs =
      ask_text::Prepare(page.texts);
  bar->ask.plan = ask_text::PlanQuestion(bar->text, paragraphs);
  bar->ask.isPageCut = page.isCut;
  if (bar->ask.plan.chunks.empty()) {
    FailAsk(*bar, kNoTextMessage);
    return;
  }
  SendNextChunk(mcv, *bar);
}

// Enter in the question box: read the page into paragraphs, ask Copilot which
// ones answer it, and mark them.
inline void StartAsk(void* mcv, Bar& bar, const std::string& question) {
  static unsigned long long nextAskId = 1;
  CancelAsk(mcv, bar);
  ClearTarget(mcv, bar);
  SetCount(bar, "");
  void* contents = tab_list::ActiveContents(mcv);
  if (!CanSearch(contents)) {
    SetStatus(bar, StatusKind::kError, kCannotSearchMessage);
    return;
  }
  bar.text = question;
  bar.target = contents;
  bar.ask = AskRun{};
  bar.ask.id = nextAskId++;
  SetBusy(bar, true);
  SetStatus(bar, StatusKind::kInfo, kReadingMessage);
  const unsigned long long askId = bar.ask.id;
  void* view = bar.view;
  const bool isRunning =
      edge::RunScript(contents, find_scripts::ReadParagraphsScript(),
                      [mcv, view, askId](const edge::ScriptResult& result) {
                        OnParagraphsRead(mcv, view, askId, result);
                      });
  if (!isRunning) {
    FailAsk(bar, kCannotSearchMessage);
  }
}

// --- opening and closing -------------------------------------------------------------------------

inline void StartTimer() {
  if (channel::g_window) {
    SetTimer(channel::g_window, kTimerId, kPollIntervalMs, nullptr);
  }
}

inline bool IsAnyOpen() {
  for (const auto& [mcv, bar] : Bars()) {
    if (bar.isOpen) {
      return true;
    }
  }
  return false;
}

inline void StopTimerIfIdle() {
  if (!IsAnyOpen() && channel::g_window) {
    KillTimer(channel::g_window, kTimerId);
  }
}

// The bar's rectangle: the top right of the page, beside an open Gemini panel
// rather than under it.
inline Rect BarBounds(void* mcv) {
  Rect content = edge::ViewBounds(mcv);
  content.width -= reserve::WidthFor(mcv);
  return find_layout::BarRect(content);
}

inline void Reposition(void* mcv, const Bar& bar) {
  const Rect wanted = BarBounds(mcv);
  if (edge::ViewBounds(bar.view) != wanted) {
    edge::SetViewBounds(bar.view, wanted);
  }
  assistant_chat::Reposition(mcv);
}

// Give the keyboard back to the page that was being searched.
inline void FocusPage(void* mcv) {
  const void* active = tab_list::ActiveContents(mcv);
  const size_t containerCount = edge::McvCount(mcv);
  for (size_t i = 0; i < containerCount; i++) {
    void* container = edge::McvContainerAt(mcv, static_cast<int>(i));
    const bool isActivePane =
        active && edge::ContainerWebContents(container) == active;
    if (isActivePane) {
      edge::ViewRequestFocus(edge::ContainerWebView(container));
      return;
    }
  }
}

inline Bar* EnsureBar(HWND window, void* mcv) {
  if (Bar* existing = BarFor(mcv)) {
    return existing;
  }
  void* tsm = edge::McvTabStripModel(mcv);
  void* profile = tsm ? edge::ProfileForTabStrip(tsm) : nullptr;
  void* parent = edge::ViewParent(mcv);
  if (!profile || !parent) {
    return nullptr;
  }
  void* view = edge::NewWebView(profile);
  edge::SetViewVisible(view, false);
  edge::AddChildViewAt(parent, view, edge::ViewChildCount(parent));
  const std::string url = share_text::BuildDataUrl(find_page::Html());
  edge::WebViewLoadUrl(view, url.c_str());
  Bar& bar = Bars()[mcv];
  bar.view = view;
  bar.parent = parent;
  bar.window = window;
  Log("[find] bar created view=%p parent=%p", view, parent);
  return &bar;
}

inline void OnSelectionRead(void* mcv, void* view,
                            const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  const bool isWaiting = bar && bar->openStep == OpenStep::kReadingSelection;
  if (!isWaiting) {
    return;
  }
  bar->selection = result.isString ? result.text : std::string();
  bar->openStep = OpenStep::kReady;
}

// Whether Ctrl+F in `window` should open our bar: the feature is on and the
// active tab is a page whose text our scripts can search. Anything else (an Edge
// page, a PDF) is left to Edge's own find.
inline bool CanOpenFor(HWND window) {
  if (!g_isEnabled) {
    return false;
  }
  void* mcv = edge::McvForWindow(window);
  void* contents = mcv ? tab_list::ActiveContents(mcv) : nullptr;
  return CanSearch(contents);
}

// Show the bar (or, when it is already up, bring it back to the box with the
// page's selection in it). False when Edge's own find should open instead.
inline bool Open(HWND window) {
  if (!CanOpenFor(window)) {
    Log("[find] left to Edge's own find (enabled=%d)",
        static_cast<int>(g_isEnabled));
    return false;
  }
  void* mcv = edge::McvForWindow(window);
  Bar* bar = EnsureBar(window, mcv);
  if (!bar) {
    return false;
  }
  bar->isOpen = true;
  edge::SetViewVisible(bar->view, true);
  Reposition(mcv, *bar);
  edge::ViewRequestFocus(bar->view);
  bar->openStep = OpenStep::kReadingSelection;
  bar->selection.clear();
  void* view = bar->view;
  const bool isReading = edge::RunScript(
      tab_list::ActiveContents(mcv), find_scripts::SelectionScript(),
      [mcv, view](const edge::ScriptResult& result) {
        OnSelectionRead(mcv, view, result);
      });
  if (!isReading) {
    bar->openStep = OpenStep::kReady;
  }
  StartTimer();
  Log("[find] bar opened");
  return true;
}

inline void Close(void* mcv) {
  Bar* bar = BarFor(mcv);
  if (!bar || !bar->isOpen) {
    return;
  }
  CancelAsk(mcv, *bar);
  ClearTarget(mcv, *bar);
  assistant_chat::Cancel(mcv);
  bar->isOpen = false;
  bar->openStep = OpenStep::kNone;
  edge::SetViewVisible(bar->view, false);
  FocusPage(mcv);
  StopTimerIfIdle();
  Log("[find] bar closed");
}

// --- the timer -------------------------------------------------------------------------------------

inline void HandleEvent(void* mcv, Bar& bar, const find_text::Event& event) {
  switch (event.kind) {
    case find_text::EventKind::kQuery: {
      bar.options = event.options;
      bar.text = event.text;
      CancelAsk(mcv, bar);
      const bool isAsking = event.options.mode == find_text::Mode::kAsk;
      if (isAsking) {
        ClearTarget(mcv, bar);
        SetCount(bar, "");
        SetStatus(bar, StatusKind::kInfo, kAskPromptMessage);
      } else {
        RunSearch(mcv, bar);
      }
      return;
    }
    case find_text::EventKind::kNext:
      StepBy(mcv, bar, find_text::Step::kNext);
      return;
    case find_text::EventKind::kPrevious:
      StepBy(mcv, bar, find_text::Step::kPrevious);
      return;
    case find_text::EventKind::kAsk:
      bar.options = event.options;
      StartAsk(mcv, bar, event.text);
      return;
    case find_text::EventKind::kClose:
      Close(mcv);
      return;
  }
}

inline void OnPolled(void* mcv, void* view, const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  if (!bar) {
    return;
  }
  bar->isPolling = false;
  const find_text::Report report = result.isString
                                       ? find_text::ParseReport(result.text)
                                       : find_text::Report{};
  if (!report.isInstalled) {
    bar->isInstalled = false;
    return;
  }
  for (const find_text::Event& event : report.events) {
    HandleEvent(mcv, *bar, event);
    const bool isStillOpen = OpenBarOf(mcv, view) != nullptr;
    if (!isStillOpen) {
      return;
    }
  }
}

inline void OnInstalled(void* mcv, void* view,
                        const edge::ScriptResult& result) {
  Bar* bar = OpenBarOf(mcv, view);
  if (!bar) {
    return;
  }
  bar->isPolling = false;
  bar->isInstalled =
      result.isString && result.text == find_page::kInstalledAnswer;
}

// A tab switch while the bar is up: the marks stay in the old tab (cleared if it
// is still there), and the new tab is searched for what the box holds.
inline void FollowActiveTab(void* mcv, Bar& bar) {
  const bool isTabChanged =
      bar.target && tab_list::ActiveContents(mcv) != bar.target;
  if (!isTabChanged) {
    return;
  }
  const bool isAsking = bar.options.mode == find_text::Mode::kAsk;
  CancelAsk(mcv, bar);
  ClearTarget(mcv, bar);
  SetCount(bar, "");
  if (isAsking) {
    SetStatus(bar, StatusKind::kInfo, kTabChangedMessage);
  } else {
    RunSearch(mcv, bar);
  }
}

inline void TickBar(void* mcv, Bar& bar) {
  Reposition(mcv, bar);
  void* contents = edge::WebViewContents(bar.view);
  const bool isBarReady = contents && edge::IsPageLive(contents);
  if (!isBarReady) {
    bar.isInstalled = false;
    return;
  }
  if (bar.isPolling) {
    return;
  }
  void* view = bar.view;
  if (!bar.isInstalled) {
    bar.isPolling = true;
    const bool isRunning =
        edge::RunScript(contents, find_page::InstallScript(),
                        [mcv, view](const edge::ScriptResult& result) {
                          OnInstalled(mcv, view, result);
                        });
    bar.isPolling = isRunning;
    return;
  }
  if (bar.openStep == OpenStep::kReady) {
    bar.openStep = OpenStep::kNone;
    CallBar(bar, "window.__ff.open(" +
                     share_text::JsStringLiteral(bar.selection) + ")");
    return;
  }
  FollowActiveTab(mcv, bar);
  bar.isPolling = true;
  const bool isRunning =
      edge::RunScript(contents, find_page::PollScript(),
                      [mcv, view](const edge::ScriptResult& result) {
                        OnPolled(mcv, view, result);
                      });
  bar.isPolling = isRunning;
}

// One timer tick (control.h's window procedure): move Copilot's question on a
// step and poll every open bar.
inline void Tick() {
  DropStale();
  assistant_chat::Tick();
  for (auto& [mcv, bar] : Bars()) {
    if (bar.isOpen) {
      TickBar(mcv, bar);
    }
  }
}

// --- the switch and teardown ----------------------------------------------------------------------

inline void RemoveAll() {
  DropStale();
  std::vector<void*> windows;
  for (const auto& [mcv, bar] : Bars()) {
    windows.push_back(mcv);
  }
  for (void* mcv : windows) {
    Close(mcv);
  }
  assistant_chat::RemoveAll();
  for (auto& [mcv, bar] : Bars()) {
    edge::ViewRemoveChild(bar.parent, bar.view);
    edge::DeleteWebView(bar.view);
  }
  Bars().clear();
  if (channel::g_window) {
    KillTimer(channel::g_window, kTimerId);
  }
}

// The panel switched the find bar on or off.
inline void Apply(bool isEnabled) {
  const bool isTurningOff = g_isEnabled && !isEnabled;
  const bool isChanging = g_isEnabled != isEnabled;
  if (isChanging) {
    Log("[find] switched %s", isEnabled ? "on" : "off");
  }
  g_isEnabled = isEnabled;
  if (isTurningOff) {
    RemoveAll();
  }
}

}  // namespace fixedge::find
