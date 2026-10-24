// find.h — the enhanced find, through Edge's own find bar, which stays the only
// UI. A RegExp, or a question for the browser assistant (Copilot) about which
// paragraphs answer it, is searched by us instead of Edge; anything else is
// Edge's own search, untouched. Matches are drawn with the CSS Custom Highlight
// API (find_scripts.h), so the page's DOM is not touched, and the count and any
// status go in the bar's own label.
#pragma once
#include <windows.h>
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "MinHook.h"
#include "anim.h"
#include "ask_text.h"
#include "assistant.h"
#include "assistant_chat.h"
#include "channel.h"
#include "edge.h"
#include "find_flags.h"
#include "find_scripts.h"
#include "find_text.h"
#include "handles.h"
#include "tab_list.h"

namespace fixedge::find {

inline constinit bool g_isEnabled = false;

inline constexpr UINT_PTR kTimerId =
    channel::TimerIdOf(channel::WindowTimer::kFindPoll);
inline constexpr UINT kTickIntervalMs = 100;

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
  return IsWindow(session.window) && edge::McvForWindow(session.window) == mcv;
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
  const bool isSameBar = session && session->findBarView == view;
  return isSameBar ? session : nullptr;
}

inline edge::U16View Utf16Of(const std::u16string& text) {
  return edge::U16View{text.data(), text.size()};
}

inline std::string TextOf(const edge::U16View view) {
  return edge::Utf16ToUtf8(view);
}

inline std::u16string Utf16FromUtf8(const std::string& utf8) {
  edge::U16String converted{};
  edge::Utf8ToUtf16(&converted, utf8.c_str());
  const edge::U16View view = edge::U16StringView(&converted);
  const std::u16string text(view.data, view.length);
  edge::FreeU16String(&converted);
  return text;
}

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
    const bool isSpinning = session.isBusy && session.findBarView;
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

// The switches are the bar's own views, so their state is per window.
inline find_text::Option OptionOf(void* findBarView) {
  if (!g_isEnabled) {
    return find_text::Option::kNone;
  }
  void* regExp =
      edge::FindToggle(findBarView, edge155::off::FINDTOGGLE_id_regexp);
  void* ask = edge::FindToggle(findBarView, edge155::off::FINDTOGGLE_id_ask);
  if (regExp && edge::FindToggleIsOn(regExp)) {
    return find_text::Option::kRegExp;
  }
  if (ask && edge::FindToggleIsOn(ask)) {
    return find_text::Option::kAsk;
  }
  return find_text::Option::kNone;
}

inline void* EnsureToggle(void* findBarView, int toggleId, bool* isAdded) {
  void* toggle = edge::FindToggle(findBarView, toggleId);
  if (!toggle && g_isEnabled) {
    toggle = edge::AddFindToggle(findBarView, toggleId);
    *isAdded = *isAdded || toggle != nullptr;
  }
  return toggle;
}

inline int ToggleIdOfFlag(find_flags::Flag flag) {
  switch (flag) {
    case find_flags::Flag::kIndices:
      return edge155::off::FINDTOGGLE_id_flag_indices;
    case find_flags::Flag::kIgnoreCase:
      return edge155::off::FINDTOGGLE_id_flag_ignore_case;
    case find_flags::Flag::kMultiline:
      return edge155::off::FINDTOGGLE_id_flag_multiline;
    case find_flags::Flag::kDotAll:
      return edge155::off::FINDTOGGLE_id_flag_dot_all;
    case find_flags::Flag::kUnicode:
      return edge155::off::FINDTOGGLE_id_flag_unicode;
    case find_flags::Flag::kUnicodeSets:
      return edge155::off::FINDTOGGLE_id_flag_unicode_sets;
    case find_flags::Flag::kSticky:
      return edge155::off::FINDTOGGLE_id_flag_sticky;
  }
  return edge155::off::FINDTOGGLE_id_flag_indices;
}

inline const find_flags::FlagInfo* FlagOfToggleId(int toggleId) {
  for (const find_flags::FlagInfo& info : find_flags::kFlags) {
    if (ToggleIdOfFlag(info.flag) == toggleId) {
      return &info;
    }
  }
  return nullptr;
}

inline bool IsFlagToggleId(int toggleId) {
  return FlagOfToggleId(toggleId) != nullptr;
}

inline bool IsOurToggleId(int toggleId) {
  return toggleId == edge155::off::FINDTOGGLE_id_regexp ||
         toggleId == edge155::off::FINDTOGGLE_id_ask ||
         IsFlagToggleId(toggleId);
}

inline find_flags::FlagSet FlagsOn(void* findBarView) {
  find_flags::FlagSet set;
  for (const find_flags::FlagInfo& info : find_flags::kFlags) {
    void* toggle = edge::FindToggle(findBarView, ToggleIdOfFlag(info.flag));
    if (toggle && edge::FindToggleIsOn(toggle)) {
      find_flags::SetOn(&set, info.flag, true);
    }
  }
  return set;
}

// The flag switches count only while RegExp is the chosen switch: they are
// hidden otherwise, so a stale one must never reach a shortcut such as
// /pattern/.
inline find_text::Query QueryOf(void* findBarView, const std::string& typed) {
  const find_text::Option option = OptionOf(findBarView);
  find_text::Query query = find_text::ParseQuery(typed, option);
  const bool isUsingFlagSwitches = option == find_text::Option::kRegExp &&
                                   query.mode == find_text::Mode::kRegExp;
  if (isUsingFlagSwitches) {
    query.flags = find_flags::Merge(query.flags, FlagsOn(findBarView));
    query.isFlagsFinal = true;
  }
  return query;
}

// With the RegExp switch on, the box reads like a JavaScript literal: "/" before
// the text field and "/" plus the flags after it. They are two plain Labels set
// into the bar's row on either side of the text field, so the caret, selection,
// clipboard and searched text never see them, and the row mirrors them in a
// right-to-left UI. They are found again by where they sit (Edge has no plain
// Label there), so a reloaded DLL takes up the ones it made before.

struct Decorations {
  void* prefix = nullptr;
  void* suffix = nullptr;
};

inline bool TextfieldPlace(void* findBarView, void** row, size_t* index) {
  void* textfield = edge::FindBarTextfield(findBarView);
  void* parent = textfield ? edge::ViewParent(textfield) : nullptr;
  const size_t count = parent ? edge::ViewChildCount(parent) : 0;
  for (size_t i = 0; i < count; i++) {
    if (edge::ViewChildAt(parent, i) == textfield) {
      *row = parent;
      *index = i;
      return true;
    }
  }
  return false;
}

inline void* PlainLabelAt(void* row, size_t index) {
  const bool isInRow = index < edge::ViewChildCount(row);
  void* child = isInRow ? edge::ViewChildAt(row, index) : nullptr;
  return child && edge::IsPlainLabel(child) ? child : nullptr;
}

inline Decorations FindDecorations(void* findBarView) {
  void* row = nullptr;
  size_t index = 0;
  if (!TextfieldPlace(findBarView, &row, &index)) {
    return Decorations{};
  }
  void* prefix = index > 0 ? PlainLabelAt(row, index - 1) : nullptr;
  return Decorations{prefix, PlainLabelAt(row, index + 1)};
}

// The vtables name the children's classes in the PDB: the evidence for where the
// slashes went.
inline void LogRow(void* row) {
  const size_t count = edge::ViewChildCount(row);
  for (size_t i = 0; i < count; i++) {
    Log("[find]   row child %zu vtable %#llx", i,
        static_cast<unsigned long long>(
            edge::VtableRva(edge::ViewChildAt(row, i))));
  }
}

// The box's own font gives the typed text's size and baseline; the count's
// colour is Edge's dimmed secondary text in either theme.
inline void* NewDecorationLabel(void* findBarView) {
  void* countLabel = edge::FindBarCountLabel(findBarView);
  void* label = edge::NewLabelLike(countLabel);
  edge::LabelCopyEnabledColor(label, countLabel);
  edge::LabelUseTextfieldFont(label, edge::FindBarTextfield(findBarView));
  return label;
}

inline Decorations EnsureDecorations(void* findBarView) {
  const Decorations found = FindDecorations(findBarView);
  const bool isComplete = found.prefix && found.suffix;
  const void* countLabel = edge::FindBarCountLabel(findBarView);
  void* row = nullptr;
  size_t index = 0;
  const bool canAdd =
      !isComplete && countLabel && TextfieldPlace(findBarView, &row, &index);
  if (!canAdd) {
    return found;
  }
  if (!found.prefix) {
    edge::AddChildViewAt(row, NewDecorationLabel(findBarView), index);
    index++;
  }
  if (!found.suffix) {
    edge::AddChildViewAt(row, NewDecorationLabel(findBarView), index + 1);
  }
  Log("[find] slash labels added around the box, row %p (vtable %#llx)", row,
      static_cast<unsigned long long>(edge::VtableRva(row)));
  LogRow(row);
  return FindDecorations(findBarView);
}

// So a bar's row is laid out again only on a change.
inline std::map<void*, std::string>& ShownSuffixes() {
  static std::map<void*, std::string> suffixes;
  return suffixes;
}

inline void SyncDecorations(void* findBarView) {
  const bool isShowing = OptionOf(findBarView) == find_text::Option::kRegExp;
  const Decorations decorations =
      isShowing ? EnsureDecorations(findBarView) : FindDecorations(findBarView);
  if (!decorations.prefix || !decorations.suffix) {
    return;
  }
  const find_text::LiteralDecoration literal = find_text::RegExpDecoration(
      TextOf(edge::FindBarText(findBarView)), FlagsOn(findBarView));
  const std::string shown = isShowing ? literal.suffix : std::string();
  std::string& lastShown = ShownSuffixes()[findBarView];
  const bool isVisibilityChanged =
      edge::ViewGetVisible(decorations.prefix) != isShowing;
  if (!isVisibilityChanged && shown == lastShown) {
    return;
  }
  lastShown = shown;
  Log("[find] slashes %s, flag switches on \"%s\"",
      isShowing ? shown.c_str() : "hidden",
      find_flags::Letters(FlagsOn(findBarView)).c_str());
  if (isShowing) {
    edge::LabelSetText(decorations.prefix,
                       Utf16Of(Utf16FromUtf8(literal.prefix)));
    edge::LabelSetText(decorations.suffix, Utf16Of(Utf16FromUtf8(shown)));
  }
  edge::SetViewVisible(decorations.prefix, isShowing);
  edge::SetViewVisible(decorations.suffix, isShowing);
  edge::InvalidateLayout(edge::ViewParent(decorations.prefix));
  edge::FindBarRelayout(findBarView);
}

// Edge's own three switches are greyed out while either of ours is on, since
// they mean nothing to a RegExp or a question.
inline void SyncTogglesNow(void* findBarView) {
  bool isAdded = false;
  void* regExp =
      EnsureToggle(findBarView, edge155::off::FINDTOGGLE_id_regexp, &isAdded);
  // Built right after RegExp so they sit under it.
  const bool isRegExpOn = regExp && edge::FindToggleIsOn(regExp);
  const bool isShowingFlags = g_isEnabled && isRegExpOn;
  bool isFlagVisibilityChanged = false;
  for (const find_flags::FlagInfo& info : find_flags::kFlags) {
    const int flagToggleId = ToggleIdOfFlag(info.flag);
    const bool isNewToggle = !edge::FindToggle(findBarView, flagToggleId);
    void* flagToggle = EnsureToggle(findBarView, flagToggleId, &isAdded);
    if (!flagToggle) {
      continue;
    }
    const bool isStartingOn = isNewToggle && find_flags::IsDefaultOn(info.flag);
    if (isStartingOn) {
      edge::FindToggleSetOn(flagToggle, true);
    }
    const std::u16string title = Utf16FromUtf8(find_flags::SwitchTitle(info));
    edge::FindToggleSetTitle(flagToggle, Utf16Of(title));
    isFlagVisibilityChanged =
        isFlagVisibilityChanged ||
        edge::ViewGetVisible(flagToggle) != isShowingFlags;
    edge::SetViewVisible(flagToggle, isShowingFlags);
  }
  void* ask =
      EnsureToggle(findBarView, edge155::off::FINDTOGGLE_id_ask, &isAdded);
  if (regExp) {
    const std::u16string title = Utf16FromUtf8(find_text::kRegExpName);
    edge::FindToggleSetTitle(regExp, Utf16Of(title));
    edge::SetViewVisible(regExp, g_isEnabled);
  }
  if (ask) {
    const std::u16string title =
        Utf16FromUtf8(find_text::AskTitle(assistant::Name()));
    edge::FindToggleSetTitle(ask, Utf16Of(title));
    edge::SetViewVisible(ask, g_isEnabled);
  }
  // RegExp and Ask each grey out every other switch while on.
  const find_text::Option option = OptionOf(findBarView);
  if (regExp) {
    edge::FindToggleSetEnabled(regExp, option != find_text::Option::kAsk);
  }
  if (ask) {
    edge::FindToggleSetEnabled(ask, option != find_text::Option::kRegExp);
  }
  const bool isEdgeLocked = option != find_text::Option::kNone;
  const int edgeIds[] = {edge155::off::FINDTOGGLE_id_match_case,
                         edge155::off::FINDTOGGLE_id_whole_word,
                         edge155::off::FINDTOGGLE_id_diacritics};
  for (const int toggleId : edgeIds) {
    void* toggle = edge::FindToggle(findBarView, toggleId);
    if (toggle) {
      edge::FindToggleSetEnabled(toggle, !isEdgeLocked);
    }
  }
  if (isAdded || isFlagVisibilityChanged) {
    edge::FindBarRelayoutOptions(findBarView);
  }
  SyncDecorations(findBarView);
}

// Building a switch makes Edge set its state (h_active_contents_changed), which
// syncs again; that inner call is dropped, the outer one finishes the job.
inline constinit bool g_isSyncingToggles = false;
inline void SyncToggles(void* findBarView) {
  if (g_isSyncingToggles) {
    return;
  }
  g_isSyncingToggles = true;
  SyncTogglesNow(findBarView);
  g_isSyncingToggles = false;
}

// A question needs room, so with Ask on the bar is as wide again as Edge makes
// it, anchored by its right edge as Edge anchors it; the box takes the room,
// since it is the bar's flexible view.
inline constexpr int kAskWidthFactor = 2;
inline constexpr int kBarMarginPixels = 24;
inline constexpr int kBarMaxWidthPixels = 1100;  // wider than any window's bar

// The bar has a top-level window of its own: untitled and far narrower than the
// browser window.
inline HWND BarWindowOf(HWND browserWindow) {
  struct Search {
    HWND browser;
    HWND found;
  } search{browserWindow, nullptr};
  EnumThreadWindows(
      GetCurrentThreadId(),
      [](HWND candidate, LPARAM parameter) -> BOOL {
        auto* inner = reinterpret_cast<Search*>(parameter);
        RECT candidateRect{};
        RECT browserRect{};
        GetWindowRect(candidate, &candidateRect);
        GetWindowRect(inner->browser, &browserRect);
        const bool isNarrow = candidateRect.right - candidateRect.left <
                              (browserRect.right - browserRect.left) / 2;
        const bool isBar = candidate != inner->browser &&
                           IsWindowVisible(candidate) &&
                           GetWindowTextLengthW(candidate) == 0 && isNarrow;
        if (isBar) {
          inner->found = candidate;
        }
        return isBar ? FALSE : TRUE;
      },
      reinterpret_cast<LPARAM>(&search));
  return search.found;
}

inline void StartTimer();

inline void ApplyBarWidth(Session& session, bool isAsking) {
  HWND bar = BarWindowOf(session.window);
  RECT barRect{};
  RECT browserRect{};
  const bool isMeasured = bar && GetWindowRect(bar, &barRect) &&
                          GetWindowRect(session.window, &browserRect);
  if (!isMeasured) {
    return;
  }
  const int current = barRect.right - barRect.left;
  const bool isHeldWide =
      session.widenedBarWidth != 0 && current == session.widenedBarWidth;
  if (!isHeldWide) {
    session.stockBarWidth = current;
  }
  const int roomForBar =
      browserRect.right - browserRect.left - 2 * kBarMarginPixels;
  const int widest = std::min(kBarMaxWidthPixels, roomForBar);
  const int wanted =
      isAsking
          ? std::max(session.stockBarWidth,
                     std::min(session.stockBarWidth * kAskWidthFactor, widest))
          : session.stockBarWidth;
  session.widenedBarWidth = isAsking ? wanted : 0;
  if (isAsking) {
    // Edge puts its own width back whenever it lays the bar out again; the
    // timer's tick (Tick) holds it.
    StartTimer();
  }
  if (wanted == current) {
    return;
  }
  SetWindowPos(bar, nullptr, barRect.right - wanted, barRect.top, wanted,
               barRect.bottom - barRect.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

// A tab that was closed must never be run a script.
inline bool IsTabAlive(void* mcv, void* contents) {
  return contents && tab_list::FindTab(mcv, tab_list::TabIdOf(contents));
}

inline bool CanSearch(void* contents) {
  return contents && edge::IsPageLive(contents) &&
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
  const bool isCurrentSearch = session && session->searchSeq == seq;
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
  const bool isOtherTab = session.target && session.target != contents;
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
  const bool isCurrentSearch = session && session->searchSeq == seq;
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
  const bool hasMatches = session.count > 0 && session.target;
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

inline void EndAsk(Session& session) {
  session.ask = AskRun{};
  assistant_chat::ReleasePanel(edge::McvForWindow(session.window));
  // The timer stops with the ask, so a spinner left up would freeze.
  const bool isSpinnerLeft = session.isBusy && session.findBarView;
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
  const bool isCurrentAsk = session && session->ask.id == askId;
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
  const bool isCurrentAsk = session && session->ask.id == askId;
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
  const bool isCurrentAsk = session && session->ask.id == askId;
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
  const bool isCurrentAsk = session && session->ask.id == askId;
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

inline void StartTimer() {
  if (channel::g_window) {
    SetTimer(channel::g_window, kTimerId, kTickIntervalMs, nullptr);
  }
}

inline bool IsAnyAsking() {
  for (const auto& [mcv, session] : Sessions()) {
    const bool isHoldingWidth =
        session.findBarView && session.widenedBarWidth != 0;
    if (session.ask.id != 0 || isHoldingWidth) {
      return true;
    }
  }
  return false;
}

inline void StopTimerIfIdle() {
  if (!IsAnyAsking() && channel::g_window) {
    KillTimer(channel::g_window, kTimerId);
  }
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

inline void Reset(void* mcv, Session& session) {
  CancelAsk(mcv, session);
  ClearTarget(mcv, session);
  session.askedText.clear();
}

inline void Close(void* mcv) {
  Session* session = SessionFor(mcv);
  if (!session) {
    return;
  }
  Reset(mcv, *session);
  assistant_chat::Cancel(mcv);
  session->widenedBarWidth = 0;
  session->stockBarWidth = 0;
  session->findBarView = nullptr;
  StopTimerIfIdle();
  Log("[find] bar closed");
}

// Edge draws its find bar in a window of its own that also resolves to the
// browser's views, so a candidate must have a title (the bar's window has none):
// a session tied to the bar's window would be dropped the moment it changes.
inline bool IsBrowserWindow(HWND candidate) {
  const bool hasTitle = GetWindowTextLengthW(candidate) > 0;
  return candidate && hasTitle && edge::McvForWindow(candidate);
}

inline HWND BrowserWindowInUse() {
  HWND foreground = GetForegroundWindow();
  HWND owner = GetAncestor(foreground, GA_ROOTOWNER);
  for (HWND candidate : {owner, foreground}) {
    if (IsBrowserWindow(candidate)) {
      return candidate;
    }
  }
  const bool isSubclassedUsable =
      channel::g_window && edge::McvForWindow(channel::g_window);
  return isSubclassedUsable ? channel::g_window : nullptr;
}

inline Session* SessionOfBar(void* findBarView, void** mcvOut) {
  HWND window = BrowserWindowInUse();
  void* mcv = window ? edge::McvForWindow(window) : nullptr;
  if (!mcv) {
    return nullptr;
  }
  DropStale();
  Session& session = Sessions()[mcv];
  const bool isNewBar = session.findBarView != findBarView;
  if (isNewBar) {
    Reset(mcv, session);
    session.findBarView = findBarView;
    SyncToggles(findBarView);
  }
  session.window = window;
  *mcvOut = mcv;
  return &session;
}

using Find_t = void(__fastcall*)(void*, const edge::U16View*);
using FindNext_t = void(__fastcall*)(void*, bool);
using HandleKeyEvent_t = bool(__fastcall*)(void*, void*, const void*);
using EndFindSession_t = void(__fastcall*)(void*, int, int);
inline constinit Find_t o_find = nullptr;
inline constinit FindNext_t o_find_next = nullptr;
inline constinit HandleKeyEvent_t o_handle_key_event = nullptr;
inline constinit EndFindSession_t o_end_find_session = nullptr;
using AddOptionsView_t = void(__fastcall*)(void*, bool);
using ToggleOptions_t = void(__fastcall*)(void*, void*);
using ButtonToggled_t = void(__fastcall*)(void*);
inline constinit AddOptionsView_t o_add_options_view = nullptr;
inline constinit ToggleOptions_t o_toggle_options = nullptr;
inline constinit ButtonToggled_t o_button_toggled = nullptr;
using ActiveContentsChanged_t = void(__fastcall*)(void*);
inline constinit ActiveContentsChanged_t o_active_contents_changed = nullptr;

// The text changed. For our queries Edge's own marks and count are cleared first
// by asking it to find nothing; a question waits for Enter.
inline void __fastcall h_find(void* findBarView, const edge::U16View* typed) {
  if (!g_isEnabled) {
    o_find(findBarView, typed);
    return;
  }
  void* mcv = nullptr;
  Session* session = SessionOfBar(findBarView, &mcv);
  if (session) {
    ApplyBarWidth(*session, OptionOf(findBarView) == find_text::Option::kAsk);
  }
  // A typed (?i) changes the flags the closing slash shows.
  SyncDecorations(findBarView);
  const find_text::Query query = QueryOf(findBarView, TextOf(*typed));
  const bool isOurs = session && query.mode != find_text::Mode::kText;
  if (!isOurs) {
    if (session) {
      Reset(mcv, *session);
    }
    o_find(findBarView, typed);
    return;
  }
  const edge::U16View nothing{u"", 0};
  o_find(findBarView, &nothing);
  Reset(mcv, *session);
  if (query.mode == find_text::Mode::kRegExp) {
    RunSearch(mcv, *session, query);
  } else {
    ShowStatus(findBarView, HintStatus());
  }
}

// Enter, Shift+Enter or an arrow button. A question is asked on the first Enter
// and stepped through afterwards. False leaves the press to Edge.
inline bool HandleNext(void* findBarView, bool isReverse) {
  if (!g_isEnabled) {
    return false;
  }
  void* mcv = nullptr;
  Session* session = SessionOfBar(findBarView, &mcv);
  const std::string typed = TextOf(edge::FindBarText(findBarView));
  const find_text::Query query = QueryOf(findBarView, typed);
  const bool isOurs = session && query.mode != find_text::Mode::kText;
  if (!isOurs) {
    return false;
  }
  const find_text::Step step =
      isReverse ? find_text::Step::kPrevious : find_text::Step::kNext;
  const bool isOtherTab =
      session->target && session->target != tab_list::ActiveContents(mcv);
  if (isOtherTab) {
    Reset(mcv, *session);
  }
  if (query.mode == find_text::Mode::kRegExp) {
    const bool isSearched = session->target != nullptr;
    if (isSearched) {
      StepBy(mcv, *session, step);
    } else {
      RunSearch(mcv, *session, query);
    }
    return true;
  }
  const bool isAsking = session->ask.id != 0;
  const bool isAnswered = session->askedText == typed;
  const bool isQuestionEmpty = query.pattern.empty();
  if (isAsking || isQuestionEmpty) {
    return true;
  }
  if (isAnswered) {
    StepBy(mcv, *session, step);
  } else {
    StartAsk(mcv, *session, typed, query.pattern);
  }
  return true;
}

inline void __fastcall h_find_next(void* findBarView, bool isReverse) {
  if (!HandleNext(findBarView, isReverse)) {
    o_find_next(findBarView, isReverse);
  }
}

// Posted to the window with the keyboard focus, which is the bar's, so the text
// replaces a selection like typing.
inline void TypeIntoBar(const std::string& text) {
  HWND bar = GetFocus();
  if (!bar) {
    return;
  }
  for (const char16_t character : Utf16FromUtf8(text)) {
    PostMessageW(bar, WM_CHAR, character, 1);
  }
}

inline std::string ClipboardText() {
  std::string text;
  const handles::OpenedClipboard clipboard;
  if (!clipboard.IsOpen()) {
    return text;
  }
  const handles::GlobalMemoryLock data{GetClipboardData(CF_UNICODETEXT)};
  const auto* wide = static_cast<const wchar_t*>(data.Data());
  if (wide) {
    text = TextOf(edge::U16View{reinterpret_cast<const char16_t*>(wide),
                                std::char_traits<wchar_t>::length(wide)});
  }
  return text;
}

inline constexpr uint8_t kKeyV = 'V';
inline constexpr uint8_t kKeyInsert = VK_INSERT;

// The box is a single-line text field, so with RegExp on a line break goes in as
// the two characters \n: Shift+Enter (or Ctrl+Enter) types one instead of
// stepping back, and a multi-line paste is typed with each as \n (Edge's own
// paste would drop or flatten them).
inline bool HandleLineBreakKey(void* findBarView, const void* keyEvent) {
  const uint8_t key = edge::PressedKeyCode(keyEvent);
  const bool isRegExpOn = key != 0 && g_isEnabled &&
                          OptionOf(findBarView) == find_text::Option::kRegExp;
  if (!isRegExpOn) {
    return false;
  }
  const bool isShiftDown = GetKeyState(VK_SHIFT) < 0;
  const bool isCtrlDown = GetKeyState(VK_CONTROL) < 0;
  const bool isNewlineKey = key == VK_RETURN && (isShiftDown || isCtrlDown);
  if (isNewlineKey) {
    TypeIntoBar(find_text::kEscapedNewline);
    return true;
  }
  const bool isPasteKey =
      (key == kKeyV && isCtrlDown) || (key == kKeyInsert && isShiftDown);
  if (!isPasteKey) {
    return false;
  }
  const std::string pasted = ClipboardText();
  const bool isMultiline = pasted.find_first_of("\r\n") != std::string::npos;
  if (!isMultiline) {
    return false;
  }
  TypeIntoBar(find_text::EscapeLineBreaks(pasted).substr(
      0, find_text::kMaxPatternChars));
  return true;
}

// Edge turns Enter in the text field into a search of its own without going
// through FindNext.
inline bool __fastcall h_handle_key_event(void* controller, void* textfield,
                                          const void* keyEvent) {
  void* findBarView = edge::FindBarViewOfKeyController(controller);
  if (HandleLineBreakKey(findBarView, keyEvent)) {
    return true;
  }
  const bool isEnterPressed = g_isEnabled && edge::IsEnterPressed(keyEvent);
  const bool isShiftDown = GetKeyState(VK_SHIFT) < 0;
  const bool isHandled = isEnterPressed && HandleNext(findBarView, isShiftDown);
  return isHandled || o_handle_key_event(controller, textfield, keyEvent);
}

inline void __fastcall h_add_options_view(void* findBarView, bool isArgument) {
  o_add_options_view(findBarView, isArgument);
  SyncToggles(findBarView);
}

inline void __fastcall h_toggle_options(void* findBarView, void* argument) {
  o_toggle_options(findBarView, argument);
  SyncToggles(findBarView);
}

// A pattern or a question means nothing to Match case, Match whole word or Match
// diacritics: switch off the ones that are on. Edge's own callback follows the move, so the
// find parameters it keeps for them stay in step with what the bar shows.
inline void SwitchOffEdgeToggles(void* findBarView) {
  const int edgeIds[] = {edge155::off::FINDTOGGLE_id_match_case,
                         edge155::off::FINDTOGGLE_id_whole_word,
                         edge155::off::FINDTOGGLE_id_diacritics};
  for (const int toggleId : edgeIds) {
    void* toggle = edge::FindToggle(findBarView, toggleId);
    const bool isOn = toggle && edge::FindToggleIsOn(toggle);
    if (isOn) {
      edge::FindToggleSetOn(toggle, false);
      o_button_toggled(toggle);
    }
  }
}

// One of the switches was clicked; Edge's three go on as Edge has them.
inline void __fastcall h_button_toggled(void* toggle) {
  const int toggleId = edge::FindToggleId(toggle);
  const bool isOurs = g_isEnabled && IsOurToggleId(toggleId);
  void* findBarView = isOurs ? edge::FindBarViewOfToggle(toggle) : nullptr;
  if (!findBarView) {
    o_button_toggled(toggle);
    return;
  }
  // The switch has already moved by the time its callback runs.
  const bool isTurningOn = edge::FindToggleIsOn(toggle);
  const find_flags::FlagInfo* flag = FlagOfToggleId(toggleId);
  const bool isFlag = flag != nullptr;
  const int otherId = toggleId == edge155::off::FINDTOGGLE_id_regexp
                          ? edge155::off::FINDTOGGLE_id_ask
                          : edge155::off::FINDTOGGLE_id_regexp;
  void* other = edge::FindToggle(findBarView, otherId);
  if (isTurningOn && other && !isFlag) {
    edge::FindToggleSetOn(other, false);
  }
  find_flags::Flag excluded = find_flags::Flag::kIndices;
  const bool isExcludingOther =
      isFlag && isTurningOn && find_flags::HasExclusive(flag->flag, &excluded);
  if (isExcludingOther) {
    void* excludedToggle =
        edge::FindToggle(findBarView, ToggleIdOfFlag(excluded));
    if (excludedToggle) {
      edge::FindToggleSetOn(excludedToggle, false);
    }
  }
  const bool isTurningModeOn = isTurningOn && !isFlag;
  if (isTurningModeOn) {
    SwitchOffEdgeToggles(findBarView);
  }
  SyncToggles(findBarView);
  const edge::U16View typed = edge::FindBarText(findBarView);
  h_find(findBarView, &typed);
}

// A switch is set again from the tab's find options, as the bar is shown again
// or the tab changes. Edge keeps no state for our ids and so turned them off
// here, with no click callback: RegExp was off on reopen while its flags and the
// slashes still showed. Ours keep the state that lives in the switch.
inline void __fastcall h_active_contents_changed(void* toggle) {
  const int toggleId = edge::FindToggleId(toggle);
  const bool isOurs = g_isEnabled && IsOurToggleId(toggleId);
  if (!isOurs) {
    o_active_contents_changed(toggle);
  }
  void* findBarView = g_isEnabled ? edge::FindBarViewOfToggle(toggle) : nullptr;
  if (findBarView) {
    SyncToggles(findBarView);
  }
}

inline void __fastcall h_end_find_session(void* controller, int action,
                                          int isKeepingSelection) {
  o_end_find_session(controller, action, isKeepingSelection);
  std::vector<void*> windows;
  for (const auto& [mcv, session] : Sessions()) {
    windows.push_back(mcv);
  }
  for (void* mcv : windows) {
    Close(mcv);
  }
}

struct HookSite {
  uintptr_t rva;
  void* detour;
  void** original;
};

inline std::vector<HookSite> HookSites() {
  return {
      {edge155::rva::FindBarView_Find, reinterpret_cast<void*>(h_find),
       reinterpret_cast<void**>(&o_find)},
      {edge155::rva::FindBarView_FindNext, reinterpret_cast<void*>(h_find_next),
       reinterpret_cast<void**>(&o_find_next)},
      {edge155::rva::FindBarView_HandleKeyEvent,
       reinterpret_cast<void*>(h_handle_key_event),
       reinterpret_cast<void**>(&o_handle_key_event)},
      {edge155::rva::FindBarController_EndFindSession,
       reinterpret_cast<void*>(h_end_find_session),
       reinterpret_cast<void**>(&o_end_find_session)},
      {edge155::rva::FindBarView_EdgeMaybeAddFindOptionsView,
       reinterpret_cast<void*>(h_add_options_view),
       reinterpret_cast<void**>(&o_add_options_view)},
      {edge155::rva::FindBarView_EdgeToggleFindOptions,
       reinterpret_cast<void*>(h_toggle_options),
       reinterpret_cast<void**>(&o_toggle_options)},
      {edge155::rva::FindInPageOptionToggleView_OnButtonToggled,
       reinterpret_cast<void*>(h_button_toggled),
       reinterpret_cast<void**>(&o_button_toggled)},
      {edge155::rva::FindInPageOptionToggleView_EdgeOnActiveWebContentsChanged,
       reinterpret_cast<void*>(h_active_contents_changed),
       reinterpret_cast<void**>(&o_active_contents_changed)},
  };
}

// Re-enabled after the split's MH_ALL_HOOKS turns everything off; with the find
// switched off they pass through.
inline bool EnableAll() {
  bool isEnabled = true;
  for (const HookSite& site : HookSites()) {
    const MH_STATUS status = MH_EnableHook(edge::At(site.rva));
    isEnabled = isEnabled && (status == MH_OK || status == MH_ERROR_ENABLED);
  }
  return isEnabled;
}

inline bool Install() {
  bool isHooked = true;
  for (const HookSite& site : HookSites()) {
    isHooked = isHooked && MH_CreateHook(edge::At(site.rva), site.detour,
                                         site.original) == MH_OK;
  }
  if (!isHooked) {
    Log("[err] find hooks failed");
    return false;
  }
  const bool isEnabled = EnableAll();
  Log("[ok] find hooks installed (%d)", static_cast<int>(isEnabled));
  return isEnabled;
}

inline void Tick() {
  DropStale();
  for (auto& [mcv, session] : Sessions()) {
    const bool isBarOpen = session.findBarView && session.widenedBarWidth != 0;
    if (isBarOpen) {
      ApplyBarWidth(session,
                    OptionOf(session.findBarView) == find_text::Option::kAsk);
    }
  }
  TurnSpinners();
  assistant_chat::Tick();
  StopTimerIfIdle();
}

inline void RemoveAll() {
  DropStale();
  std::vector<void*> windows;
  for (const auto& [mcv, session] : Sessions()) {
    windows.push_back(mcv);
  }
  for (void* mcv : windows) {
    Close(mcv);
  }
  assistant_chat::RemoveAll();
  Sessions().clear();
  if (channel::g_window) {
    KillTimer(channel::g_window, kTimerId);
  }
}

inline void Apply(bool isEnabled) {
  const bool isTurningOff = g_isEnabled && !isEnabled;
  const bool isChanging = g_isEnabled != isEnabled;
  if (isChanging) {
    Log("[find] switched %s", isEnabled ? "on" : "off");
  }
  DropStale();
  std::vector<void*> bars;
  for (const auto& [mcv, session] : Sessions()) {
    if (session.findBarView) {
      bars.push_back(session.findBarView);
    }
  }
  g_isEnabled = isEnabled;
  if (isTurningOff) {
    RemoveAll();
  }
  if (isChanging) {
    for (void* findBarView : bars) {
      SyncToggles(findBarView);
    }
  }
}

}  // namespace fixedge::find
