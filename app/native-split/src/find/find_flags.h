// find_flags.h — the RegExp flags the find bar's options offer as switches, and
// merging them with the flags typed after a closing slash. Pure.
//
// What each one does in our matcher (find_scripts.h runs it in the page):
//   d  match indices: asked for, never shown (a highlight needs none)
//   g  every match is found and marked (always on, not a switch)
//   m  ^ and $ stand for the start and end of a line (a page's text is one line
//      per block); off, for the start and end of the whole page text
//   u, v  cannot both be on, as in JavaScript
//   y  sticky: the first match is found anywhere, then each next match must
//      start exactly where the one before it ended (a contiguous run)
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace edgeplus::find_flags {

// In JavaScript's canonical flag order, which is also the order of the
// switches in the bar and of the letters in the flags string.
enum class Flag {
  kIndices,
  kIgnoreCase,
  kMultiline,
  kDotAll,
  kUnicode,
  kUnicodeSets,
  kSticky,
};

struct FlagInfo {
  Flag flag = Flag::kIndices;
  char letter = '\0';
  const char* title = "";  // without the letter
};

inline constexpr std::array<FlagInfo, 7> kFlags = {{
    {Flag::kIndices, 'd', "Match indices"},
    {Flag::kIgnoreCase, 'i', "Ignore case"},
    {Flag::kMultiline, 'm', "Multiline"},
    {Flag::kDotAll, 's', "Dot matches newline"},
    {Flag::kUnicode, 'u', "Unicode"},
    {Flag::kUnicodeSets, 'v', "Unicode sets"},
    {Flag::kSticky, 'y', "Sticky"},
}};

struct FlagSet {
  unsigned bits = 0;
};

[[nodiscard]] inline constexpr unsigned BitOf(Flag flag) noexcept {
  return 1u << static_cast<unsigned>(flag);
}

inline constexpr char kAlwaysOnFlag = 'g';

// m, exactly as the search behaved before there were switches.
[[nodiscard]] inline constexpr FlagSet DefaultFlags() noexcept {
  return FlagSet{BitOf(Flag::kMultiline)};
}

[[nodiscard]] inline constexpr bool IsDefaultOn(Flag flag) noexcept {
  return (DefaultFlags().bits & BitOf(flag)) != 0;
}

[[nodiscard]] inline constexpr bool IsOn(const FlagSet& set,
                                         Flag flag) noexcept {
  return (set.bits & BitOf(flag)) != 0;
}

[[nodiscard]] inline constexpr bool HasExclusive(Flag flag,
                                                 Flag* other) noexcept {
  if (flag == Flag::kUnicode) {
    *other = Flag::kUnicodeSets;
    return true;
  }
  if (flag == Flag::kUnicodeSets) {
    *other = Flag::kUnicode;
    return true;
  }
  return false;
}

inline constexpr void SetOn(FlagSet* set, Flag flag, bool isOn) noexcept {
  if (!isOn) {
    set->bits &= ~BitOf(flag);
    return;
  }
  Flag other = flag;
  if (HasExclusive(flag, &other)) {
    set->bits &= ~BitOf(other);
  }
  set->bits |= BitOf(flag);
}

[[nodiscard]] inline std::string Letters(const FlagSet& set) {
  std::string letters;
  for (const FlagInfo& info : kFlags) {
    if (IsOn(set, info.flag)) {
      letters.push_back(info.letter);
    }
  }
  return letters;
}

// Typed letters that are not flags are dropped (the typed form has already
// refused them). A typed u next to a switched-on v (or the reverse) is kept: the
// page's RegExp refuses the pair and the bar says so.
[[nodiscard]] inline std::string Merge(std::string_view typedFlags,
                                       const FlagSet& set) {
  std::string merged;
  for (const FlagInfo& info : kFlags) {
    const bool isTyped = typedFlags.find(info.letter) != std::string_view::npos;
    if (isTyped || IsOn(set, info.flag)) {
      merged.push_back(info.letter);
    }
  }
  // g sorts right after d, the only flag before it.
  const bool hasIndices = !merged.empty() && merged[0] == kFlags[0].letter;
  merged.insert(hasIndices ? 1 : 0, 1, kAlwaysOnFlag);
  return merged;
}

// Two em spaces: a flag's switch sits under RegExp as a sub-group.
inline constexpr char kSubGroupIndent[] = "\xE2\x80\x83\xE2\x80\x83";

[[nodiscard]] inline std::string SwitchTitle(const FlagInfo& info) {
  return std::string(kSubGroupIndent) + info.title + " (" + info.letter + ")";
}

}  // namespace edgeplus::find_flags
