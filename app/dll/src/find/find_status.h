// find_status.h — what the bar's own label says: the count, or a status (a
// hint, reading, asking, waiting, a failure) with a spinner while busy.
#pragma once
#include <windows.h>
#include <string>
#include "assistant/assistant.h"
#include "edge/api/find_bar.h"
#include "edge/api/strings.h"
#include "edge/api/views.h"
#include "find/find_bar_text.h"
#include "find/find_poll.h"
#include "find/find_session.h"
#include "find/find_text.h"
#include "layout/anim.h"

namespace edgeplus::find {

// The bar's label is small, so each status is a short label plus, for the
// tooltip, the sentence behind it. A busy status has no words in the label, only
// a spinner: words there read as part of the question and take the box's room.
struct Status {
  std::string label;
  std::string detail;
  bool isBusy = false;
};

// Braille dots turning, one frame per tick; a still ellipsis with Windows'
// animation effects off. Code points, since the source is not read as UTF-8.
inline constexpr char16_t kSpinnerFrames[] = {0x280B, 0x2819, 0x2839, 0x2838,
                                              0x283C, 0x2834, 0x2826, 0x2827,
                                              0x2807, 0x280F};
inline constexpr char16_t kStillBusyMark = 0x2026;

// No label, so the box keeps its room.
inline Status HintStatus() {
  return {"", "Press Enter to ask " + std::string(assistant::Name()) +
                  " about this page."};
}
inline Status ReadingStatus() {
  return {"", "Reading the page's paragraphs...", true};
}
inline Status AskingStatus(size_t part, size_t partCount) {
  const std::string progress = partCount > 1
                                   ? " (part " + std::to_string(part) + " of " +
                                         std::to_string(partCount) + ")"
                                   : std::string();
  return {"", "Asking " + std::string(assistant::Name()) + progress + "...",
          true};
}
inline Status WaitingStatus() {
  return {"", "Waiting for " + std::string(assistant::Name()) + "...", true};
}
inline Status NeedsUserStatus() {
  return {"Accept notice",
          "Accept the " + std::string(assistant::Name()) +
              " notice in its side panel to continue; the question goes "
              "through once you have."};
}
inline Status NoParagraphStatus(const std::string& why) {
  return {"No answer", why};
}
inline Status FailedStatus(const std::string& why) {
  return {"Failed", why};
}
inline Status BadRegExpStatus(const std::string& why) {
  return {std::string("Bad ") + find_text::kRegExpName, why};
}
inline Status CappedStatus() {
  return {"", "More than " + std::to_string(find_text::kMaxMatches) +
                  " matches: the first " +
                  std::to_string(find_text::kMaxMatches) + " are marked."};
}

inline constexpr char kCannotSearchMessage[] =
    "This page cannot be searched here.";
inline constexpr char kNoAnswerMessage[] = "The page did not answer.";
inline constexpr char kNoTextMessage[] = "This page has no text to ask about.";
inline constexpr char kTabChangedMessage[] =
    "The tab changed. Ask again for this page.";
inline constexpr char kPageChangedMessage[] = "The page changed. Ask again.";
inline constexpr char kLongPageMessage[] =
    " Only the first part of this long page was searched.";

inline std::u16string SpinnerFrame() {
  if (!anim::g_isEnabled) {
    return std::u16string(1, kStillBusyMark);
  }
  const ULONGLONG tick = GetTickCount64() / kTickIntervalMs;
  return std::u16string(1, kSpinnerFrames[tick % std::size(kSpinnerFrames)]);
}

inline void ShowStatus(void* findBarView, const Status& status) {
  const std::u16string label =
      status.isBusy ? SpinnerFrame() : Utf16FromUtf8(status.label);
  edge::FindBarSetLabel(findBarView, Utf16Of(label));
  edge::ViewSetTooltip(edge::FindBarLabel(findBarView), status.detail.c_str());
  for (auto& [mcv, session] : Sessions()) {
    if (session.findBarView == findBarView) {
      session.isBusy = status.isBusy;
    }
  }
}

// The timer's tick turns the spinner of every busy bar.
inline void TurnSpinners() {
  for (const auto& [mcv, session] : Sessions()) {
    const bool isSpinning = session.isBusy && (session.findBarView != nullptr);
    if (isSpinning) {
      edge::FindBarSetLabel(session.findBarView, Utf16Of(SpinnerFrame()));
    }
  }
}

inline void ShowCount(Session& session) {
  session.isBusy = false;
  const edge::U16View boxText = edge::FindBarText(session.findBarView);
  const int ordinal = session.current < 0 ? 0 : session.current + 1;
  edge::FindBarShowResult(session.findBarView, session.count, ordinal, boxText);
  const Status tooltip = session.isCapped ? CappedStatus() : Status{};
  edge::ViewSetTooltip(edge::FindBarLabel(session.findBarView),
                       tooltip.detail.c_str());
}

inline void ClearLabel(Session& session) {
  ShowStatus(session.findBarView, Status{});
}

}  // namespace edgeplus::find
