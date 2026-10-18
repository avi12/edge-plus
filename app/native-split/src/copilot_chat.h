// copilot_chat.h — asking Copilot a question from inside Edge. One concern: a
// Copilot web chat of our own per browser window (a WebView in the window's
// profile, so a user signed in to Edge is signed in here too), and the steps of
// one question: wait for its message box, send the prompt, wait for the answer
// to stop growing, hand the answer back. It is hidden unless Copilot needs the
// user (a sign-in or a consent page), when it is shown under the find bar so they
// can deal with it; the question goes through once they have. Edge ships no API
// for this: copilot_scripts.h drives the web app the way a person would.
#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include "ask_text.h"
#include "copilot_scripts.h"
#include "edge.h"
#include "find_layout.h"

namespace fixedge::copilot_chat {

inline constexpr char kCopilotUrl[] = "https://copilot.microsoft.com/";

// How long to look for the message box before asking the user to sign in, how
// long to keep looking in all, how long an answer may take, and how often each
// step is tried.
inline constexpr ULONGLONG kBoxWaitMs = 12000;
inline constexpr ULONGLONG kGiveUpOnBoxMs = 180000;
inline constexpr ULONGLONG kReplyTimeoutMs = 120000;
// How many times the prompt is sent again after the page was replaced under it.
inline constexpr int kMaxResends = 3;
// How long a dialog may wait for the person before the question is given up.
inline constexpr ULONGLONG kUserWaitMs = 300000;
inline constexpr ULONGLONG kSendRetryMs = 500;
inline constexpr ULONGLONG kReplyPollMs = 700;

// What the caller is told besides the answer.
enum class Notice {
  kNeedsUser,  // Copilot shows no message box: it is shown for the user
  kSent,       // the prompt went into the box; now waiting for the answer
};

enum class Phase {
  kIdle,
  kWaitingForBox,
  kWaitingForReply,
};

using DoneFn = std::function<void(const ask_text::Reply&)>;
using NoticeFn = std::function<void(Notice)>;

struct Chat {
  void* view = nullptr;
  void* parent = nullptr;
  HWND window = nullptr;
  Phase phase = Phase::kIdle;
  unsigned long long token =
      0;  // names the question; a late answer to an older one is dropped
  ULONGLONG phaseStartedAt = 0;
  ULONGLONG nextTryAt = 0;
  bool isScriptRunning = false;
  bool isShownForUser = false;
  ULONGLONG blockedSince = 0;  // when a dialog first waited for the person
  int resendCount = 0;         // times the page was replaced after the send
  std::string prompt;
  DoneFn onDone;
  NoticeFn onNotice;
};

inline std::map<void*, Chat>& Chats() {
  static std::map<void*, Chat> chats;
  return chats;
}

// Whether `chat` still belongs to `mcv`: a closed window destroyed its views.
inline bool IsLive(void* mcv, const Chat& chat) {
  return IsWindow(chat.window) && edge::McvForWindow(chat.window) == mcv;
}

inline void DropStale() {
  for (auto entry = Chats().begin(); entry != Chats().end();) {
    if (IsLive(entry->first, entry->second)) {
      ++entry;
    } else {
      entry = Chats().erase(entry);
    }
  }
}

inline Chat* ChatFor(void* mcv) {
  DropStale();
  const auto entry = Chats().find(mcv);
  return entry == Chats().end() ? nullptr : &entry->second;
}

// `chat` if it is `mcv`'s and is still working on question `token`.
inline Chat* Current(void* mcv, unsigned long long token) {
  Chat* chat = ChatFor(mcv);
  const bool isCurrent =
      chat && chat->token == token && chat->phase != Phase::kIdle;
  return isCurrent ? chat : nullptr;
}

inline void SetShownForUser(void* mcv, Chat& chat, bool isShown) {
  chat.isShownForUser = isShown;
  edge::SetViewVisible(chat.view, isShown);
  if (isShown) {
    edge::SetViewBounds(chat.view,
                        find_layout::ChatRect(edge::ViewBounds(mcv)));
  }
}

// Keep a shown page under the bar through a resize.
inline void Reposition(void* mcv) {
  const Chat* chat = ChatFor(mcv);
  if (chat && chat->isShownForUser) {
    edge::SetViewBounds(chat->view,
                        find_layout::ChatRect(edge::ViewBounds(mcv)));
  }
}

// The question ends: hide the page and tell the caller.
inline void Finish(void* mcv, Chat& chat, const ask_text::Reply& reply) {
  const DoneFn done = std::move(chat.onDone);
  chat.phase = Phase::kIdle;
  chat.onDone = nullptr;
  chat.onNotice = nullptr;
  chat.prompt.clear();
  if (chat.isShownForUser) {
    SetShownForUser(mcv, chat, false);
  }
  if (done) {
    done(reply);
  }
}

inline void Fail(void* mcv, Chat& chat, const std::string& why) {
  ask_text::Reply reply;
  reply.error = why;
  Finish(mcv, chat, reply);
}

// The chat page for `mcv`, built on first use: a hidden WebView docked as the
// last child of the MCV's parent (so, when shown, it draws over the page).
inline Chat* EnsureChat(HWND window, void* mcv) {
  if (Chat* existing = ChatFor(mcv)) {
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
  edge::WebViewLoadUrl(view, kCopilotUrl);
  Chat& chat = Chats()[mcv];
  chat.view = view;
  chat.parent = parent;
  chat.window = window;
  Log("[copilot] chat page created view=%p", view);
  return &chat;
}

// Start asking: `prompt` goes to Copilot, and `onDone` gets its answer (or why
// there is none) later, on the UI thread. False if the window has no profile to
// open Copilot in. One question at a time per window: a new one replaces the
// old (whose callback is never called).
inline bool Ask(HWND window, void* mcv, const std::string& prompt,
                DoneFn onDone, NoticeFn onNotice) {
  static unsigned long long nextToken = 1;
  Chat* chat = EnsureChat(window, mcv);
  if (!chat) {
    return false;
  }
  if (chat->isShownForUser) {
    SetShownForUser(mcv, *chat, false);
  }
  chat->token = nextToken++;
  chat->phase = Phase::kWaitingForBox;
  chat->phaseStartedAt = GetTickCount64();
  chat->nextTryAt = 0;
  chat->isScriptRunning = false;
  chat->blockedSince = 0;
  chat->resendCount = 0;
  chat->prompt = prompt;
  chat->onDone = std::move(onDone);
  chat->onNotice = std::move(onNotice);
  return true;
}

// Give up on the question in flight without telling anyone (the bar closed).
inline void Cancel(void* mcv) {
  Chat* chat = ChatFor(mcv);
  if (!chat) {
    return;
  }
  chat->phase = Phase::kIdle;
  chat->onDone = nullptr;
  chat->onNotice = nullptr;
  chat->prompt.clear();
  if (chat->isShownForUser) {
    SetShownForUser(mcv, *chat, false);
  }
}

// --- the steps ---------------------------------------------------------------------------

inline void OnSendAnswer(void* mcv, unsigned long long token,
                         const edge::ScriptResult& result) {
  Chat* chat = Current(mcv, token);
  if (!chat) {
    return;
  }
  chat->isScriptRunning = false;
  const bool isScheduled =
      result.isString && result.text == copilot_scripts::kScheduledAnswer;
  if (isScheduled) {
    chat->phase = Phase::kWaitingForReply;
    chat->phaseStartedAt = GetTickCount64();
    chat->nextTryAt = GetTickCount64() + kReplyPollMs;
    if (chat->isShownForUser) {
      SetShownForUser(mcv, *chat, false);
    }
    if (chat->onNotice) {
      chat->onNotice(Notice::kSent);
    }
    return;
  }
  chat->nextTryAt = GetTickCount64() + kSendRetryMs;
}

inline void OnReplyAnswer(void* mcv, unsigned long long token,
                          const edge::ScriptResult& result) {
  Chat* chat = Current(mcv, token);
  if (!chat) {
    return;
  }
  chat->isScriptRunning = false;
  const ULONGLONG now = GetTickCount64();
  chat->nextTryAt = now + kReplyPollMs;
  const bool isNeedingUser =
      result.isString && result.text == copilot_scripts::kUserAnswer;
  if (isNeedingUser) {
    // A dialog (a security check, a consent page) waits for the person: show
    // the page, and do not count their time against the answer's.
    if (chat->blockedSince == 0) {
      chat->blockedSince = now;
    }
    chat->phaseStartedAt = now;
    if (!chat->isShownForUser) {
      SetShownForUser(mcv, *chat, true);
      if (chat->onNotice) {
        chat->onNotice(Notice::kNeedsUser);
      }
    }
    const bool hasWaitedTooLong = now - chat->blockedSince > kUserWaitMs;
    if (hasWaitedTooLong) {
      Fail(mcv, *chat, "Copilot's page is still waiting for you.");
    }
    return;
  }
  chat->blockedSince = 0;
  if (chat->isShownForUser) {
    SetShownForUser(mcv, *chat, false);
  }
  const bool isReloaded =
      result.isString && result.text == copilot_scripts::kReloadedAnswer;
  if (isReloaded) {
    const bool canSendAgain = chat->resendCount < kMaxResends;
    if (!canSendAgain) {
      Fail(mcv, *chat, "Copilot's page kept reloading before it answered.");
      return;
    }
    chat->resendCount++;
    chat->phase = Phase::kWaitingForBox;
    chat->phaseStartedAt = now;
    chat->nextTryAt = now + kSendRetryMs;
    return;
  }
  const bool isWaiting =
      !result.isString || result.text == copilot_scripts::kWaitAnswer;
  if (isWaiting) {
    return;
  }
  Finish(mcv, *chat, ask_text::ParseReply(result.text));
}

inline void StepWaitingForBox(void* mcv, Chat& chat, void* contents,
                              ULONGLONG now) {
  const ULONGLONG waited = now - chat.phaseStartedAt;
  const bool shouldAskUser = waited > kBoxWaitMs && !chat.isShownForUser;
  if (shouldAskUser) {
    SetShownForUser(mcv, chat, true);
    if (chat.onNotice) {
      chat.onNotice(Notice::kNeedsUser);
    }
  }
  if (waited > kGiveUpOnBoxMs) {
    Fail(mcv, chat,
         "Copilot never showed a message box. Sign in to Copilot, then ask "
         "again.");
    return;
  }
  const bool isDue = !chat.isScriptRunning && now >= chat.nextTryAt;
  if (!isDue) {
    return;
  }
  chat.isScriptRunning = true;
  const unsigned long long token = chat.token;
  const bool isRunning =
      edge::RunScript(contents, copilot_scripts::SendScript(chat.prompt),
                      [mcv, token](const edge::ScriptResult& result) {
                        OnSendAnswer(mcv, token, result);
                      });
  if (!isRunning) {
    chat.isScriptRunning = false;
    chat.nextTryAt = now + kSendRetryMs;
  }
}

inline void StepWaitingForReply(void* mcv, Chat& chat, void* contents,
                                ULONGLONG now) {
  if (now - chat.phaseStartedAt > kReplyTimeoutMs) {
    Fail(mcv, chat, "Copilot did not answer in time.");
    return;
  }
  const bool isDue = !chat.isScriptRunning && now >= chat.nextTryAt;
  if (!isDue) {
    return;
  }
  chat.isScriptRunning = true;
  const unsigned long long token = chat.token;
  const bool isRunning =
      edge::RunScript(contents, copilot_scripts::ReplyScript(),
                      [mcv, token](const edge::ScriptResult& result) {
                        OnReplyAnswer(mcv, token, result);
                      });
  if (!isRunning) {
    chat.isScriptRunning = false;
    chat.nextTryAt = now + kReplyPollMs;
  }
}

// One timer tick (find.h's): move every question in flight one step on.
inline void Tick() {
  DropStale();
  const ULONGLONG now = GetTickCount64();
  for (auto& [mcv, chat] : Chats()) {
    if (chat.phase == Phase::kIdle) {
      continue;
    }
    void* contents = edge::WebViewContents(chat.view);
    const bool isPageReady = contents && edge::IsPageLive(contents);
    if (!isPageReady) {
      const bool hasWaitedTooLong = now - chat.phaseStartedAt > kGiveUpOnBoxMs;
      if (hasWaitedTooLong) {
        Fail(mcv, chat, "Copilot's page did not load.");
      }
      continue;
    }
    if (chat.phase == Phase::kWaitingForBox) {
      StepWaitingForBox(mcv, chat, contents, now);
    } else {
      StepWaitingForReply(mcv, chat, contents, now);
    }
  }
}

// --- teardown -----------------------------------------------------------------------------

inline void Remove(void* mcv) {
  const auto entry = Chats().find(mcv);
  if (entry == Chats().end()) {
    return;
  }
  Chat& chat = entry->second;
  if (IsLive(mcv, chat)) {
    edge::ViewRemoveChild(chat.parent, chat.view);
    edge::DeleteWebView(chat.view);
  }
  Chats().erase(entry);
}

inline void RemoveAll() {
  while (!Chats().empty()) {
    Remove(Chats().begin()->first);
  }
}

}  // namespace fixedge::copilot_chat
