// assistant_chat.h — asking the assistant (Copilot) a question from inside Edge.
// One concern: the steps of one question. It goes to Copilot's side panel by
// the same Edge function the page's "Ask Copilot" menu runs (the panel opens and
// the question is in it, as for a person), then the panel's page is read until
// the answer stops growing, and the answer is handed back. Edge has no API that
// returns the answer, so reading the panel's page (chat_scripts.h) is ours.
#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include "ask_text.h"
#include "chat_scripts.h"
#include "chat_target.h"
#include "edge.h"

namespace fixedge::assistant_chat {

// How long the side panel may take to open, how long an answer may take, and how
// often each step is tried.
inline constexpr ULONGLONG kPanelWaitMs = 20000;
inline constexpr ULONGLONG kReplyTimeoutMs = 120000;
inline constexpr ULONGLONG kStepMs = 700;
// How many times the question is sent again after the panel was reloaded.
inline constexpr int kMaxResends = 2;
// How long a dialog may wait for the person before the question is given up.
inline constexpr ULONGLONG kUserWaitMs = 120000;

// The short title the panel is given with the question.
inline constexpr char kPanelTitle[] = "Find the answer on this page";

// What the caller is told besides the answer.
enum class Notice {
  kNeedsUser,  // a dialog in the panel waits for the person
  kSent,       // the panel has the question; now waiting for the answer
};

enum class Phase {
  kIdle,
  kWaitingForPanel,
  kWaitingForReply,
};

using DoneFn = std::function<void(const ask_text::Reply&)>;
using NoticeFn = std::function<void(Notice)>;

struct Chat {
  void* browser = nullptr;
  HWND window = nullptr;
  Phase phase = Phase::kIdle;
  unsigned long long token =
      0;  // names the question; a late answer to an older one is dropped
  ULONGLONG phaseStartedAt = 0;
  ULONGLONG nextTryAt = 0;
  bool isScriptRunning = false;
  bool isNeedingUser = false;
  ULONGLONG blockedSince = 0;  // when a dialog first waited for the person
  int resendCount = 0;         // times the question was sent again
  std::string prompt;
  DoneFn onDone;
  NoticeFn onNotice;
};

// The assistant's name, for what is said to the user about it.
inline std::string NameOf(const Chat& /*chat*/) {
  return chat_target::kCopilotTarget.name;
}

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

// The question ends: tell the caller.
inline void Finish(Chat& chat, const ask_text::Reply& reply) {
  const DoneFn done = std::move(chat.onDone);
  chat.phase = Phase::kIdle;
  chat.onDone = nullptr;
  chat.onNotice = nullptr;
  chat.prompt.clear();
  if (done) {
    done(reply);
  }
}

inline void Fail(Chat& chat, const std::string& why) {
  Log("[chat] failed: %s", why.c_str());
  ask_text::Reply reply;
  reply.error = why;
  Finish(chat, reply);
}

// Put the question in Copilot's side panel (opening it).
inline void Dispatch(Chat& chat) {
  edge::SendPromptToSidePanel(chat.browser, chat.prompt, kPanelTitle);
  chat.phase = Phase::kWaitingForPanel;
  chat.phaseStartedAt = GetTickCount64();
  chat.nextTryAt = 0;
  chat.isScriptRunning = false;
}

// Start asking: `prompt` goes to the assistant, and `onDone` gets its answer
// (or why there is none) later, on the UI thread. False if the window has no
// browser to open the panel in. One question at a time per window: a new one
// replaces the old (whose callback is never called).
inline bool Ask(HWND window, void* mcv, const std::string& prompt,
                DoneFn onDone, NoticeFn onNotice) {
  static unsigned long long nextToken = 1;
  void* browserView = edge::McvBrowserView(mcv);
  void* browser = browserView ? edge::BrowserOf(browserView) : nullptr;
  if (!browser) {
    return false;
  }
  Chat& chat = Chats()[mcv];
  chat.browser = browser;
  chat.window = window;
  chat.token = nextToken++;
  chat.isNeedingUser = false;
  chat.blockedSince = 0;
  chat.resendCount = 0;
  chat.prompt = prompt;
  chat.onDone = std::move(onDone);
  chat.onNotice = std::move(onNotice);
  Dispatch(chat);
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
}

// --- the steps ---------------------------------------------------------------------------

inline void OnArmed(void* mcv, unsigned long long token,
                    const edge::ScriptResult& result) {
  Chat* chat = Current(mcv, token);
  if (!chat) {
    return;
  }
  chat->isScriptRunning = false;
  const bool isArmed =
      result.isString && result.text == chat_scripts::kArmedAnswer;
  if (!isArmed) {
    return;
  }
  Log("[chat] panel armed");
  chat->phase = Phase::kWaitingForReply;
  chat->phaseStartedAt = GetTickCount64();
  chat->nextTryAt = GetTickCount64() + kStepMs;
  if (chat->onNotice) {
    chat->onNotice(Notice::kSent);
  }
}

inline void OnReplyAnswer(void* mcv, unsigned long long token,
                          const edge::ScriptResult& result) {
  Chat* chat = Current(mcv, token);
  if (!chat) {
    return;
  }
  chat->isScriptRunning = false;
  const ULONGLONG now = GetTickCount64();
  chat->nextTryAt = now + kStepMs;
  const bool isNeedingUser =
      result.isString && result.text == chat_scripts::kUserAnswer;
  if (isNeedingUser) {
    // A dialog (a security check, a consent page) waits for the person in the
    // panel, which is open: do not count their time against the answer's.
    if (chat->blockedSince == 0) {
      chat->blockedSince = now;
    }
    chat->phaseStartedAt = now;
    if (!chat->isNeedingUser) {
      chat->isNeedingUser = true;
      if (chat->onNotice) {
        chat->onNotice(Notice::kNeedsUser);
      }
    }
    if (now - chat->blockedSince > kUserWaitMs) {
      Fail(*chat,
           NameOf(*chat) + " is still waiting for you to accept its notice.");
    }
    return;
  }
  chat->blockedSince = 0;
  const bool isReloaded =
      result.isString && result.text == chat_scripts::kReloadedAnswer;
  if (isReloaded) {
    const bool canSendAgain = chat->resendCount < kMaxResends;
    if (!canSendAgain) {
      Fail(*chat,
           NameOf(*chat) + "'s panel kept reloading before it answered.");
      return;
    }
    chat->resendCount++;
    Dispatch(*chat);
    return;
  }
  const bool isWaiting =
      !result.isString || result.text == chat_scripts::kWaitAnswer;
  if (isWaiting) {
    return;
  }
  Finish(*chat, ask_text::ParseReply(result.text));
}

inline void StepWaitingForPanel(void* mcv, Chat& chat, const void* frame,
                                ULONGLONG now) {
  const bool isDue = !chat.isScriptRunning && now >= chat.nextTryAt;
  if (!isDue) {
    return;
  }
  chat.isScriptRunning = true;
  const unsigned long long token = chat.token;
  const bool isRunning = edge::RunScriptInFrame(
      frame, chat_scripts::ArmScript(chat_target::kCopilotTarget, chat.prompt),
      [mcv, token](const edge::ScriptResult& result) {
        OnArmed(mcv, token, result);
      });
  if (!isRunning) {
    chat.isScriptRunning = false;
    chat.nextTryAt = now + kStepMs;
  }
}

inline void StepWaitingForReply(void* mcv, Chat& chat, const void* frame,
                                ULONGLONG now) {
  if (now - chat.phaseStartedAt > kReplyTimeoutMs) {
    Fail(chat, NameOf(chat) + " did not answer in time.");
    return;
  }
  const bool isDue = !chat.isScriptRunning && now >= chat.nextTryAt;
  if (!isDue) {
    return;
  }
  chat.isScriptRunning = true;
  const unsigned long long token = chat.token;
  const bool isRunning = edge::RunScriptInFrame(
      frame, chat_scripts::ReplyScript(chat_target::kCopilotTarget),
      [mcv, token](const edge::ScriptResult& result) {
        OnReplyAnswer(mcv, token, result);
      });
  if (!isRunning) {
    chat.isScriptRunning = false;
    chat.nextTryAt = now + kStepMs;
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
    const void* frame = edge::CopilotChatFrame();
    const bool isPanelReady = frame != nullptr;
    if (!isPanelReady) {
      const bool hasWaitedTooLong = now - chat.phaseStartedAt > kPanelWaitMs;
      if (hasWaitedTooLong) {
        Fail(chat, NameOf(chat) + " isn't ready: its side panel did not open.");
      }
      continue;
    }
    if (chat.phase == Phase::kWaitingForPanel) {
      StepWaitingForPanel(mcv, chat, frame, now);
    } else {
      StepWaitingForReply(mcv, chat, frame, now);
    }
  }
}

// --- teardown -----------------------------------------------------------------------------

inline void RemoveAll() {
  Chats().clear();
}

}  // namespace fixedge::assistant_chat
