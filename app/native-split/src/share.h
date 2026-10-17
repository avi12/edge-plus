// share.h — sharing a tab with the Gemini panel. One concern: when the user
// presses "Share this tab" (or picks tabs) in the panel's strip, read those
// tabs' pages and put what they hold into Gemini's prompt box, where the user
// reads it and decides to send. Never sends, and never runs while the panel is
// closed.
//
// The strip is a page of our own, so it cannot call us: a UI-thread timer asks
// it for its clicks (strip_page.h's drain) a few times a second while a panel is
// open, and answers by running scripts in the pages (page_scripts.h). The tabs
// are read by running a script in each one's own page, in an isolated world, so
// no page can see or answer it falsely.
#pragma once
#include <windows.h>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "edge.h"
#include "gemini.h"
#include "page_scripts.h"
#include "share_text.h"
#include "strip_page.h"

namespace fixedge::share {

// The poll timer on the browser window, and how long a tab may take to answer.
inline constexpr UINT_PTR kTimerId = 0x5A4E;
inline constexpr UINT kPollIntervalMs = 150;
inline constexpr ULONGLONG kAnswerTimeoutMs = 8000;

// What the strip says. The one place its words are written.
enum class StatusKind {
  kInfo,
  kOk,
  kError
};
inline const char* StatusKindName(StatusKind kind) {
  switch (kind) {
    case StatusKind::kInfo:
      return "info";
    case StatusKind::kOk:
      return "ok";
    case StatusKind::kError:
      return "error";
  }
  return "info";
}
inline constexpr char kNoTabMessage[] = "There is no tab to share.";
inline constexpr char kTabClosedMessage[] = "That tab was closed.";
inline constexpr char kBlockedTabMessage[] =
    "Edge's own pages (settings, new tab) are not shared.";
inline constexpr char kAsleepTabMessage[] =
    "That tab is asleep. Open it once, then share it.";
inline constexpr char kNotReadableMessage[] =
    "That page is not ready to be read. Try again in a moment.";
inline constexpr char kTimeoutMessage[] = "The page took too long to answer.";
inline constexpr char kNoPromptBoxMessage[] =
    "Gemini's prompt box is not on screen yet. Sign in or wait for it to load.";

// One panel's sharing state: whether its strip's script is installed, the tabs
// still to share, and the one being read.
struct Session {
  bool isInstalled = false;
  bool isPolling = false;  // a poll or install is in flight
  std::deque<std::string> queue;
  bool isReading = false;
  unsigned long long readId =
      0;  // names the read in flight; a late answer to an older one is dropped
  ULONGLONG readStartedAt = 0;
  std::string readTitle;  // the page being put into the prompt, for the status
};

inline std::map<void*, Session>& Sessions() {
  static std::map<void*, Session> sessions;
  return sessions;
}

// The panel of `mcv` if it is still the one whose strip is `strip` and is open:
// every callback checks this, since a callback can arrive after the panel was
// closed or rebuilt.
inline gemini::Panel* OpenPanel(void* mcv, void* strip) {
  gemini::Panel* panel = gemini::PanelFor(mcv);
  const bool isSamePanel = panel && panel->strip == strip;
  return isSamePanel && gemini::IsOpen(*panel) ? panel : nullptr;
}

// --- talking to the strip --------------------------------------------------------

inline void ShowStatus(const gemini::Panel& panel, StatusKind kind,
                       const std::string& text) {
  void* contents = edge::WebViewContents(panel.strip);
  if (!contents) {
    return;
  }
  const std::string script = std::string("window.__fx && window.__fx.status(") +
                             share_text::JsStringLiteral(StatusKindName(kind)) +
                             "," + share_text::JsStringLiteral(text) + "); 0";
  edge::RunScript(contents, script, nullptr);
}

// --- the window's tabs --------------------------------------------------------------

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

inline void PushTabs(const gemini::Panel& panel,
                     const std::vector<share_text::TabEntry>& tabs) {
  void* contents = edge::WebViewContents(panel.strip);
  if (contents) {
    edge::RunScript(contents,
                    "window.__fx && window.__fx.setTabs(" +
                        share_text::BuildTabsLiteral(tabs) + "); 0",
                    nullptr);
  }
}

// --- reading one tab into the prompt ----------------------------------------------

inline void ReadNext(void* mcv);

// Gemini's prompt box took (or did not take) the text; go on to the next tab.
inline void OnInserted(void* mcv, void* strip,
                       const edge::ScriptResult& result) {
  Session& session = Sessions()[mcv];
  const std::string& title = session.readTitle;
  session.isReading = false;
  const gemini::Panel* panel = OpenPanel(mcv, strip);
  if (panel) {
    const bool isInserted =
        result.isString && result.text == page_scripts::kInsertedAnswer;
    if (isInserted) {
      ShowStatus(
          *panel, StatusKind::kOk,
          "Added \"" + title + "\" to the prompt. Review it, then send.");
    } else {
      ShowStatus(*panel, StatusKind::kError, kNoPromptBoxMessage);
      session.queue.clear();
    }
  }
  ReadNext(mcv);
}

// A tab answered with its page: build the prompt block and put it in Gemini.
inline void OnRead(void* mcv, void* strip, unsigned long long readId,
                   const edge::ScriptResult& result) {
  Session& session = Sessions()[mcv];
  const bool isCurrentRead = session.isReading && session.readId == readId;
  if (!isCurrentRead) {
    return;
  }
  const gemini::Panel* panel = OpenPanel(mcv, strip);
  if (!panel) {
    session.isReading = false;
    session.queue.clear();
    return;
  }
  const share_text::PageSnapshot snapshot =
      share_text::ParseSnapshot(result.isString ? result.text : std::string());
  if (!snapshot.isOk) {
    ShowStatus(*panel, StatusKind::kError,
               snapshot.error.empty() ? kNotReadableMessage : snapshot.error);
    session.isReading = false;
    ReadNext(mcv);
    return;
  }
  void* geminiContents = edge::WebViewContents(panel->webView);
  session.readTitle = snapshot.title.empty() ? snapshot.url : snapshot.title;
  const std::string insertScript = page_scripts::InsertIntoPromptScript(
      share_text::BuildPromptBlock(snapshot));
  const bool isRunning =
      geminiContents &&
      edge::RunScript(geminiContents, insertScript,
                      [mcv, strip](const edge::ScriptResult& inserted) {
                        OnInserted(mcv, strip, inserted);
                      });
  if (!isRunning) {
    ShowStatus(*panel, StatusKind::kError, kNoPromptBoxMessage);
    session.isReading = false;
    session.queue.clear();
  }
}

// Read the next queued tab, if none is being read. A tab that cannot be shared
// is reported and skipped.
inline void ReadNext(void* mcv) {
  Session& session = Sessions()[mcv];
  const gemini::Panel* panel = gemini::PanelFor(mcv);
  while (!session.isReading && !session.queue.empty() && panel &&
         gemini::IsOpen(*panel)) {
    const std::string tabId = session.queue.front();
    session.queue.pop_front();
    void* contents = FindTab(mcv, tabId);
    if (!contents) {
      ShowStatus(*panel, StatusKind::kError, kTabClosedMessage);
      continue;
    }
    const share_text::TabState state = StateOf(contents);
    if (state == share_text::TabState::kBlocked) {
      ShowStatus(*panel, StatusKind::kError, kBlockedTabMessage);
      continue;
    }
    if (state == share_text::TabState::kAsleep) {
      ShowStatus(*panel, StatusKind::kError, kAsleepTabMessage);
      continue;
    }
    session.readId++;
    const unsigned long long readId = session.readId;
    void* strip = panel->strip;
    session.isReading = true;
    session.readStartedAt = GetTickCount64();
    const bool isReading =
        edge::RunScript(contents, page_scripts::ReadTabScript(),
                        [mcv, strip, readId](const edge::ScriptResult& result) {
                          OnRead(mcv, strip, readId, result);
                        });
    if (!isReading) {
      session.isReading = false;
      ShowStatus(*panel, StatusKind::kError, kNotReadableMessage);
      continue;
    }
    ShowStatus(*panel, StatusKind::kInfo, "Reading the page...");
  }
}

// --- the strip's clicks ---------------------------------------------------------------

inline void RunAction(void* mcv, gemini::Panel* panel,
                      const share_text::Action& action) {
  Session& session = Sessions()[mcv];
  switch (action.kind) {
    case share_text::ActionKind::kList:
      PushTabs(*panel, ListTabs(mcv));
      return;
    case share_text::ActionKind::kShareActive: {
      void* active = ActiveContents(mcv);
      if (!active) {
        ShowStatus(*panel, StatusKind::kError, kNoTabMessage);
        return;
      }
      session.queue.push_back(TabIdOf(active));
      return;
    }
    case share_text::ActionKind::kShareTabs:
      for (const std::string& tabId : action.tabIds) {
        session.queue.push_back(tabId);
      }
      return;
  }
}

inline void OnPoll(void* mcv, void* strip, const edge::ScriptResult& result) {
  Session& session = Sessions()[mcv];
  session.isPolling = false;
  gemini::Panel* panel = OpenPanel(mcv, strip);
  if (!panel) {
    return;
  }
  const share_text::PollReport report =
      share_text::ParsePoll(result.isString ? result.text : std::string());
  if (!report.isInstalled) {
    session.isInstalled = false;
    return;
  }
  gemini::SetStripExpanded(mcv, report.isExpanded);
  for (const share_text::Action& action : report.actions) {
    RunAction(mcv, panel, action);
  }
  ReadNext(mcv);
}

// Install the strip's script once its page is up.
inline void Install(void* mcv, void* contents) {
  Session& session = Sessions()[mcv];
  session.isPolling = true;
  const bool isRunning = edge::RunScript(
      contents, strip_page::kInstallScript,
      [mcv](const edge::ScriptResult& result) {
        Session& installing = Sessions()[mcv];
        installing.isPolling = false;
        installing.isInstalled =
            result.isString && result.text == strip_page::kInstalledAnswer;
      });
  if (!isRunning) {
    session.isPolling = false;
  }
}

// Give up on a read that never answered (a hung page).
inline void ExpireStaleRead(void* mcv, Session& session,
                            const gemini::Panel& panel) {
  const bool isStale =
      session.isReading &&
      GetTickCount64() - session.readStartedAt > kAnswerTimeoutMs;
  if (isStale) {
    session.isReading = false;
    session.readId++;
    ShowStatus(panel, StatusKind::kError, kTimeoutMessage);
    ReadNext(mcv);
  }
}

// One timer tick (control.h's window procedure): poll every open panel's strip.
inline void Tick() {
  gemini::DropStale();
  for (auto& [mcv, panel] : gemini::Panels()) {
    Session& session = Sessions()[mcv];
    if (!gemini::IsOpen(panel)) {
      session.queue.clear();
      session.isReading = false;
      continue;
    }
    ExpireStaleRead(mcv, session, panel);
    void* contents = edge::WebViewContents(panel.strip);
    const bool isStripReady = contents && edge::IsPageLive(contents);
    if (!isStripReady) {
      session.isInstalled = false;
      continue;
    }
    if (session.isPolling) {
      continue;
    }
    void* strip = panel.strip;
    void* panelMcv = mcv;
    if (!session.isInstalled) {
      Install(panelMcv, contents);
      continue;
    }
    session.isPolling = true;
    const bool isRunning =
        edge::RunScript(contents, strip_page::kPollScript,
                        [panelMcv, strip](const edge::ScriptResult& result) {
                          OnPoll(panelMcv, strip, result);
                        });
    if (!isRunning) {
      session.isPolling = false;
    }
  }
}

// --- the timer -------------------------------------------------------------------------

inline HWND g_timerWindow = nullptr;

inline void StartTimer(HWND hwnd) {
  if (!g_timerWindow && hwnd) {
    g_timerWindow = hwnd;
    SetTimer(hwnd, kTimerId, kPollIntervalMs, nullptr);
  }
}

inline void StopTimer() {
  if (g_timerWindow) {
    KillTimer(g_timerWindow, kTimerId);
    g_timerWindow = nullptr;
  }
  Sessions().clear();
}

}  // namespace fixedge::share
