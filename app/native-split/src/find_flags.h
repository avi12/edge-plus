// find_flags.h — the RegExp flags the find bar's options offer as switches. One
// concern, all of it pure (no Edge, no Win32; unit-tested in
// test/find_flags_test.cpp): which flags there are, their words, which are on
// at first, which exclude each other, and merging the switched-on ones with the
// flags typed after a closing slash into one string in JavaScript's canonical
// order ("dgimsuvy"). g is never a switch: every match is always found, so the
// merged flags always carry it.
//
// What each one does in our matcher (find_scripts.h runs it in the page):
//   d  match indices: asked for, never shown (a highlight needs none)
//   g  every match is found and marked (always on, not a switch)
//   i  ignore case
//   m  ^ and $ stand for the start and end of a line (a page's text is one line
//      per block); off, for the start and end of the whole page text
//   s  . also matches a line break
//   u  Unicode mode          } cannot both be on, as in JavaScript: turning
//   v  Unicode sets mode     } one on turns the other off
//   y  sticky: the first match is found anywhere, then each next match must
//      start exactly where the one before it ended (a contiguous run)
#pragma once
#include <array>
#include <cstddef>
#include <string>

namespace fixedge::find_flags {

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
  char letter = '\0';      // the JavaScript flag
  const char* title = "";  // the switch's words in the bar, without the letter
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

// The switches that are on, one bit per flag.
struct FlagSet {
  unsigned bits = 0;
};

[[nodiscard]] inline unsigned BitOf(Flag flag) {
  return 1u << static_cast<unsigned>(flag);
}

// The flag that is always on, and so has no switch: a search marks every match.
inline constexpr char kAlwaysOnFlag = 'g';

// What a bar starts with: ^ and $ are line edges (m), exactly as the search
// behaved before there were switches.
[[nodiscard]] inline FlagSet DefaultFlags() {
  return FlagSet{BitOf(Flag::kMultiline)};
}

[[nodiscard]] inline bool IsDefaultOn(Flag flag) {
  return (DefaultFlags().bits & BitOf(flag)) != 0;
}

[[nodiscard]] inline bool IsOn(const FlagSet& set, Flag flag) {
  return (set.bits & BitOf(flag)) != 0;
}

// The flag that cannot be on together with `flag` (u and v), if there is one.
[[nodiscard]] inline bool HasExclusive(Flag flag, Flag* other) {
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

// Turn a flag on or off; turning one on turns off the one it excludes.
inline void SetOn(FlagSet* set, Flag flag, bool isOn) {
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

// The switches that are on as flag letters, canonical order.
[[nodiscard]] inline std::string Letters(const FlagSet& set) {
  std::string letters;
  for (const FlagInfo& info : kFlags) {
    if (IsOn(set, info.flag)) {
      letters.push_back(info.letter);
    }
  }
  return letters;
}

// The flags typed in the box OR the switches, plus the always-on g: each letter
// once, in canonical order. Typed letters that are not flags are dropped (the typed form has
// already refused them). A typed u next to a switched-on v (or the reverse) is
// kept as it is: the page's RegExp refuses the pair and the bar says so.
[[nodiscard]] inline std::string Merge(const std::string& typedFlags,
                                       const FlagSet& set) {
  std::string merged;
  for (const FlagInfo& info : kFlags) {
    const bool isTyped = typedFlags.find(info.letter) != std::string::npos;
    if (isTyped || IsOn(set, info.flag)) {
      merged.push_back(info.letter);
    }
  }
  // g sorts right after d, the only flag before it.
  const bool hasIndices = !merged.empty() && merged[0] == kFlags[0].letter;
  merged.insert(hasIndices ? 1 : 0, 1, kAlwaysOnFlag);
  return merged;
}

// A flag's switch sits under RegExp as a sub-group: its title is pushed in by
// em spaces so the column shows the nesting, and ends with the flag's letter.
inline constexpr char kSubGroupIndent[] = "\xE2\x80\x83\xE2\x80\x83";

[[nodiscard]] inline std::string SwitchTitle(const FlagInfo& info) {
  return std::string(kSubGroupIndent) + info.title + " (" + info.letter + ")";
}

}  // namespace fixedge::find_flags
