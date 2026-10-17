// glic.h — Chrome's "Gemini in Chrome" (glic) in an Edge panel. One concern:
// drive the glic host (glic_host_script.h) that runs inside the
// gemini.google.com/glic page. The DLL installs the patcher in the page's
// isolated world, hands it Chrome's client library (glic_library.h), keeps
// offering it to the page until the page boots it, then polls the host's DOM
// bridge: each request the host makes (the window's tabs, a tab's text, switch
// to a tab) is answered from Edge's own tabs, and changes to the window's tabs
// are pushed to it. When Google will not serve glic to this account, or the
// client never starts, the panel falls back to the web app (gemini.h).
#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include "chat_action.h"
#include "edge.h"
#include "gemini.h"
#include "glic_host_script.h"
#include "glic_library.h"
#include "glic_text.h"
#include "page_scripts.h"
#include "share_text.h"
#include "tab_list.h"

namespace fixedge::glic {

using namespace fixedge::tab_list;

// How long the web client has to register before glic is given up on, and how
// often (in polls) the library is offered to the page while it has not booted.
inline constexpr ULONGLONG kClientGraceMs = 15000;
inline constexpr int kBootEveryPolls = 5;

inline constexpr char kOkAnswer[] = "ok";
inline constexpr char kNotInstalledAnswer[] = "not-installed";
inline constexpr char kBlockedMessage[] =
    "That tab is not a web page Gemini can read.";
inline constexpr char kUnreadableMessage[] = "The tab could not be read.";
inline constexpr char kUnknownOperationMessage[] = "Edge does not do that.";

// An operation the host asks for (glic_host_script.h's `call(...)`).
enum class Operation {
  kState,
  kContext,
  kActivate,
  kCreate,
  kUnknown
};

inline Operation OperationOf(const std::string& name) {
  if (name == "state") {
    return Operation::kState;
  }
  if (name == "context") {
    return Operation::kContext;
  }
  if (name == "activate") {
    return Operation::kActivate;
  }
  if (name == "create") {
    return Operation::kCreate;
  }
  return Operation::kUnknown;
}

// One panel's host session.
struct Session {
  bool isInstalled = false;  // the patcher and the library are in the page
  bool isBusy = false;       // an install or poll is in flight
  ULONGLONG busySince = 0;
  int polls = 0;
  std::string lastState;  // the last window state pushed to the host
  // Context requests waiting for their tab's page: call id -> what to answer.
  std::map<unsigned long long, glic_text::HostTab> pendingTabs;
  std::map<unsigned long long, size_t> pendingLimits;
};

// A poll whose answer never comes (the page navigated) must not block the next.
inline constexpr ULONGLONG kBusyTimeoutMs = 5000;

inline void MarkBusy(Session& session) {
  session.isBusy = true;
  session.busySince = GetTickCount64();
}

inline std::map<void*, Session>& Sessions() {
  static std::map<void*, Session> sessions;
  return sessions;
}

// The panel of `mcv` if it is still the one showing `webView` and is open.
inline gemini::Panel* OpenGlicPanel(void* mcv, void* webView) {
  gemini::Panel* panel = gemini::PanelFor(mcv);
  const bool isSamePanel = panel && panel->webView == webView &&
                           panel->mode == gemini::PanelMode::kGlic;
  return isSamePanel && gemini::IsOpen(*panel) ? panel : nullptr;
}

// --- the window's tabs, as the host reads them ---------------------------------------

inline glic_text::HostTab HostTabOf(void* contents, const void* active) {
  glic_text::HostTab tab;
  tab.tabId = IntIdOf(contents);
  tab.url = edge::TabUrl(contents);
  tab.title = edge::TabTitle(contents);
  if (tab.title.empty()) {
    tab.title = tab.url;
  }
  tab.isActive = contents == active;
  tab.canShare = StateOf(contents) == share_text::TabState::kReady;
  return tab;
}

inline std::vector<glic_text::HostTab> HostTabs(void* mcv) {
  std::vector<glic_text::HostTab> tabs;
  void* tsm = edge::McvTabStripModel(mcv);
  const int count = tsm ? edge::TabCount(tsm) : 0;
  const void* active = ActiveContents(mcv);
  for (int i = 0; i < count; i++) {
    void* contents = edge::TabWebContentsAt(tsm, i);
    if (contents) {
      tabs.push_back(HostTabOf(contents, active));
    }
  }
  return tabs;
}

// --- answering the host -------------------------------------------------------------------

inline void Reply(void* guestContents, const std::string& callId, bool isOk,
                  const std::string& payload) {
  const std::string script = "window.__fxg && window.__fxg.reply(" + callId +
                             "," + (isOk ? "true" : "false") + "," +
                             share_text::JsStringLiteral(payload) + ")";
  edge::RunScript(guestContents, script, nullptr);
}

// A tab's text arrived (or did not): answer the context request waiting on it.
inline void OnTabRead(void* mcv, void* webView, unsigned long long callId,
                      const edge::ScriptResult& result) {
  Session& session = Sessions()[mcv];
  const auto tabEntry = session.pendingTabs.find(callId);
  const auto limitEntry = session.pendingLimits.find(callId);
  if (tabEntry == session.pendingTabs.end() ||
      limitEntry == session.pendingLimits.end()) {
    return;
  }
  const glic_text::HostTab tab = tabEntry->second;
  const size_t limit = limitEntry->second;
  session.pendingTabs.erase(tabEntry);
  session.pendingLimits.erase(limitEntry);
  const gemini::Panel* panel = OpenGlicPanel(mcv, webView);
  void* guest = panel ? edge::WebViewContents(webView) : nullptr;
  if (!guest) {
    return;
  }
  const share_text::PageSnapshot snapshot =
      share_text::ParseSnapshot(result.isString ? result.text : std::string());
  const std::string payload =
      snapshot.isOk
          ? glic_text::ContextJson(tab, snapshot, limit)
          : glic_text::ErrorJson(snapshot.error.empty() ? kUnreadableMessage
                                                        : snapshot.error);
  Reply(guest, std::to_string(callId), true, payload);
}

inline void HandleContext(void* mcv, gemini::Panel& panel,
                          const glic_text::Call& call) {
  void* guest = edge::WebViewContents(panel.webView);
  const glic_text::ContextRequest request =
      glic_text::ParseContextRequest(call.json);
  void* contents = request.tabId == glic_text::kNoTabId
                       ? ActiveContents(mcv)
                       : FindByIntId(mcv, request.tabId);
  const bool isReadable =
      contents && StateOf(contents) == share_text::TabState::kReady;
  if (!isReadable) {
    Reply(guest, call.id, true, glic_text::ErrorJson(kBlockedMessage));
    return;
  }
  const unsigned long long callId = std::stoull(call.id);
  Session& session = Sessions()[mcv];
  session.pendingTabs[callId] = HostTabOf(contents, ActiveContents(mcv));
  session.pendingLimits[callId] = request.maxBytes;
  void* webView = panel.webView;
  const bool isRunning =
      edge::RunScript(contents, page_scripts::ReadTabScript(),
                      [mcv, webView, callId](const edge::ScriptResult& result) {
                        OnTabRead(mcv, webView, callId, result);
                      });
  if (!isRunning) {
    session.pendingTabs.erase(callId);
    session.pendingLimits.erase(callId);
    Reply(guest, call.id, true, glic_text::ErrorJson(kUnreadableMessage));
  }
}

inline void HandleCall(void* mcv, gemini::Panel& panel,
                       const glic_text::Call& call) {
  void* guest = edge::WebViewContents(panel.webView);
  switch (OperationOf(call.operation)) {
    case Operation::kState:
      Reply(guest, call.id, true, glic_text::StateJson(HostTabs(mcv)));
      return;
    case Operation::kContext:
      HandleContext(mcv, panel, call);
      return;
    case Operation::kActivate: {
      void* tsm = edge::McvTabStripModel(mcv);
      const int tabId =
          static_cast<int>(glic_text::NumberField(call.json, "tabId", 0));
      void* contents = FindByIntId(mcv, tabId);
      const int index =
          tsm && contents ? edge::IndexOfWebContents(tsm, contents) : -1;
      if (index >= 0) {
        edge::ActivateTabAt(tsm, index);
      }
      Reply(guest, call.id, true, "null");
      return;
    }
    case Operation::kCreate:
      // Opening a tab for Gemini is not done yet: the host answers "no tab".
      Reply(guest, call.id, true, "null");
      return;
    case Operation::kUnknown:
      Reply(guest, call.id, false, kUnknownOperationMessage);
      return;
  }
}

// --- installing and polling -----------------------------------------------------------------

// The script that hands the host and the library to the patcher (built once:
// the library is half a megabyte).
inline const std::string& LibraryScript() {
  static const std::string script =
      "window.__fxg.setLibrary(" +
      share_text::JsStringLiteral(glic_host_script::kHostScript) + "," +
      share_text::JsStringLiteral(glic_library::Load()) + ")";
  return script;
}

inline void OnInstalled(void* mcv, void* webView,
                        const edge::ScriptResult& result) {
  Session& session = Sessions()[mcv];
  session.isBusy = false;
  session.isInstalled = result.isString;
  if (!OpenGlicPanel(mcv, webView)) {
    session.isInstalled = false;
  }
}

inline void Install(void* mcv, void* webView, void* contents) {
  Session& session = Sessions()[mcv];
  MarkBusy(session);
  const bool isRunning = edge::RunScript(
      contents, glic_host_script::kPatchScript,
      [mcv, webView](const edge::ScriptResult& installed) {
        const bool isPatcher =
            installed.isString && installed.text == kOkAnswer;
        void* guest = isPatcher ? edge::WebViewContents(webView) : nullptr;
        const bool isLibraryRunning =
            guest &&
            edge::RunScript(guest, LibraryScript(),
                            [mcv, webView](const edge::ScriptResult& sent) {
                              OnInstalled(mcv, webView, sent);
                            });
        if (!isLibraryRunning) {
          Sessions()[mcv].isBusy = false;
        }
      });
  if (!isRunning) {
    session.isBusy = false;
  }
}

// What a chat menu item asked of the glic client, once it is running: a summary
// is the client's own `invoke` with a prompt to send. Starting voice is not
// something the host can ask of the client (the client starts it from its own
// button), so "talk" just leaves the panel open with the page shared, as Chrome
// itself shares the focused tab with whatever it opens.
inline void PushPendingAction(gemini::Panel& panel) {
  const chat_action::ChatAction action = panel.pendingAction;
  panel.pendingAction = chat_action::ChatAction::kNone;
  if (action != chat_action::ChatAction::kSummarize) {
    return;
  }
  const std::string invocation =
      std::string("{\"prompts\":[") +
      share_text::JsStringLiteral(chat_action::kSummarizePrompt) +
      "],\"autoSubmit\":true}";
  edge::RunScript(edge::WebViewContents(panel.webView),
                  "window.__fxg.push(\"invoke\"," +
                      share_text::JsStringLiteral(invocation) + ")",
                  nullptr);
}

inline void OnPoll(void* mcv, void* webView, const edge::ScriptResult& result) {
  Session& session = Sessions()[mcv];
  session.isBusy = false;
  gemini::Panel* panel = OpenGlicPanel(mcv, webView);
  if (!panel) {
    return;
  }
  const glic_text::Report report =
      glic_text::ParsePoll(result.isString ? result.text : std::string());
  if (!report.isInstalled) {
    session.isInstalled = false;
    return;
  }
  const ULONGLONG elapsed = GetTickCount64() - panel->modeSince;
  if (glic_text::ShouldFallBack(report, elapsed, kClientGraceMs)) {
    Log("[glic] falling back: booted=%d client='%s' unavailable=%d",
        static_cast<int>(report.isBooted), report.client.c_str(),
        static_cast<int>(report.isUnavailable));
    Sessions().erase(mcv);
    gemini::SwitchToApp(mcv);
    return;
  }
  for (const glic_text::Call& call : report.calls) {
    HandleCall(mcv, *panel, call);
  }
  const bool isRunning = report.client == "initialized";
  if (isRunning) {
    PushPendingAction(*panel);
    const std::string state = glic_text::StateJson(HostTabs(mcv));
    if (state != session.lastState) {
      session.lastState = state;
      edge::RunScript(edge::WebViewContents(webView),
                      "window.__fxg.push(\"state\"," +
                          share_text::JsStringLiteral(state) + ")",
                      nullptr);
    }
  }
}

// One timer tick for an open panel in glic mode (share.h's timer).
inline void Tick(void* mcv, gemini::Panel& panel) {
  Session& session = Sessions()[mcv];
  void* contents = edge::WebViewContents(panel.webView);
  const bool isPageReady = contents && edge::IsPageLive(contents);
  const bool isStuck =
      session.isBusy && GetTickCount64() - session.busySince > kBusyTimeoutMs;
  if (isStuck) {
    session.isBusy = false;
  }
  if (!isPageReady || session.isBusy) {
    return;
  }
  void* webView = panel.webView;
  if (!session.isInstalled) {
    Install(mcv, webView, contents);
    return;
  }
  session.polls++;
  const bool isBootDue = session.polls % kBootEveryPolls == 1;
  const std::string script = std::string("window.__fxg ? (") +
                             (isBootDue ? "window.__fxg.tryBoot()," : "") +
                             "window.__fxg.drain()) : \"" +
                             kNotInstalledAnswer + "\"";
  MarkBusy(session);
  const bool isRunning = edge::RunScript(
      contents, script, [mcv, webView](const edge::ScriptResult& result) {
        OnPoll(mcv, webView, result);
      });
  if (!isRunning) {
    session.isBusy = false;
  }
}

inline void Forget() {
  Sessions().clear();
}

}  // namespace fixedge::glic
