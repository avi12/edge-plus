import type { ColorMode, ThemeBadge } from "#src/protocol.g.ts";

/** What a theme row says about its theme (the core's `ThemeBadge`), by name. */
export const ThemeBadgeName = {
  store: "store",
  local: "local",
  inUse: "inUse",
  installedInEdge: "installedInEdge",
  light: "light",
  dark: "dark"
} as const satisfies Record<ThemeBadge, ThemeBadge>;

/** The Windows modes a theme is set for (the core's `ColorMode`), by name. */
export const ColorModeName = {
  light: "light",
  dark: "dark"
} as const satisfies Record<ColorMode, ColorMode>;
