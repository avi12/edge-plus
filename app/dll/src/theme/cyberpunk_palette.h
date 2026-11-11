// cyberpunk_palette.h — the Cyberpunk skin's colours for the whole browser, as
// the Cyberpunk UX kit names them (README.md "Theme roles", controls.css and
// primitives.css tokens): every colour is a role, and a role has one colour per
// tone. The browser's colour ids are mapped to roles in
// chrome/colours/colour_roles.h; nothing else picks a colour. Pure.
//
// The kit's rules this enforces:
// - One colour per job: ink or cyan writes, teal draws idle lines, red is for
//   strokes, fills and marks, amber labels, yellow and green never come here.
// - Light is the same HUD inverted: ink outlines and ink text, and cyan only
//   fills (kOnFill), so kOn is ink there.
// - Red never carries text: no text role is red, and the alert text is the
//   lifted red (dark) or the deep one (light).
// - Cyan is data, so nothing floats on it: menus, bubbles, dialogs and
//   tooltips are never cyan fills (kPanel, kTooltipBackground).
#pragma once
#include <cstdint>
#include "theme/theme_tone.h"

namespace edgeplus::cyberpunk_palette {

using theme_tone::Argb;
using theme_tone::Blend;
using theme_tone::Tone;

enum class Role : uint8_t {
  kNone = 0,
  kPanel,            // floating and field surfaces: menus, bubbles, fields
  kForeground,       // body text
  kForegroundMuted,  // secondary text, placeholders
  kOn,               // icons, values, links, the focus ring: cyan / ink
  kOff,              // idle strokes, disabled marks, separators
  kFrame,            // a plate's outline: red in dark, ink in light
  kOnFill,           // a lit fill (a primary button): cyan in both tones
  kOnFillWash,       // the faint cyan fill of a switched-on track
  kInk,              // text on kOnFill
  kAccent,           // red marks: a checked box, the picked radio
  kLabel,            // amber field and section labels
  kHover,            // the wash behind a hovered or selected row
  kSelection,    // selected text: the accent red, as the selected tab's frame
  kOnSelection,  // text on kSelection
  kAlertText,    // an error line
  kAlert,        // an invalid field's frame
  kTooltipBackground,  // near-black (dark) / ink (light)
  kTooltipForeground,  // cyan (dark) / amber (light)
};

struct Palette {
  Argb panel;
  Argb foreground;
  Argb foregroundMuted;
  Argb on;
  Argb off;
  Argb frame;
  Argb onFill;
  Argb ink;
  Argb accent;
  Argb label;
  Argb hover;
  Argb selection;
  Argb onSelection;
  Argb alertText;
  Argb alert;
  Argb tooltipBackground;
  Argb tooltipForeground;
};

// The kit's dark neon register: controls.css dark tokens, --fg and --fg-muted
// from the skin's token sheet.
inline constexpr Palette kDark{
    .panel = 0xff05080c,
    .foreground = 0xffe2f4fa,
    .foregroundMuted = 0xff93b2c2,
    .on = 0xff56feff,
    .off = 0xff007979,
    .frame = 0xfff72944,
    .onFill = 0xff56feff,
    .ink = 0xff06182b,
    .accent = 0xfff72944,
    .label = 0xffffd540,
    .hover = 0x38007979,      // #007979 at 22%
    .selection = 0x47f72944,  // the tab frame's red at 28%: a fill, never text
    .onSelection = 0xffe2f4fa,  // body text stays on top
    .alertText = 0xffff7a8a,
    .alert = 0xffff142b,
    .tooltipBackground = 0xeb000000,  // black at 92%
    .tooltipForeground = 0xff56feff,
};

// The same HUD inverted: ink writes and outlines, cyan only fills.
inline constexpr Palette kLight{
    .panel = 0xffedf5fc,
    .foreground = 0xff06182b,
    .foregroundMuted = 0xff2a4a66,
    .on = 0xff06182b,
    .off = 0xff547a99,
    .frame = 0xff06182b,
    .onFill = 0xff56feff,
    .ink = 0xff06182b,
    .accent = 0xffc8102e,
    .label = 0xff6e5200,
    .hover = 0x1406182b,      // ink at 8%
    .selection = 0x47c8102e,  // the tab frame's red at 28%: a fill, never text
    .onSelection = 0xff06182b,  // ink stays on top
    .alertText = 0xffa3001f,
    .alert = 0xffc4004e,
    .tooltipBackground = 0xff06182b,
    .tooltipForeground = 0xffffd540,
};

// A switched-on track: the kit's faint cyan fill, 22% of the lit fill over
// the panel.
inline constexpr uint32_t kOnWashPercent = 22;

// A tone left unknown is taken as dark: the kit's neon register.
[[nodiscard]] constexpr const Palette& PaletteFor(Tone tone) noexcept {
  return tone == Tone::kLight ? kLight : kDark;
}

[[nodiscard]] constexpr Argb ColourOf(Role role, Tone tone) noexcept {
  const Palette& palette = PaletteFor(tone);
  switch (role) {
    case Role::kPanel:
      return palette.panel;
    case Role::kForeground:
      return palette.foreground;
    case Role::kForegroundMuted:
      return palette.foregroundMuted;
    case Role::kOn:
      return palette.on;
    case Role::kOff:
      return palette.off;
    case Role::kFrame:
      return palette.frame;
    case Role::kOnFill:
      return palette.onFill;
    case Role::kOnFillWash:
      return Blend(palette.panel, palette.onFill, kOnWashPercent);
    case Role::kInk:
      return palette.ink;
    case Role::kAccent:
      return palette.accent;
    case Role::kLabel:
      return palette.label;
    case Role::kHover:
      return palette.hover;
    case Role::kSelection:
      return palette.selection;
    case Role::kOnSelection:
      return palette.onSelection;
    case Role::kAlertText:
      return palette.alertText;
    case Role::kAlert:
      return palette.alert;
    case Role::kTooltipBackground:
      return palette.tooltipBackground;
    case Role::kTooltipForeground:
      return palette.tooltipForeground;
    case Role::kNone:
      break;
  }
  return 0;
}

}  // namespace edgeplus::cyberpunk_palette
