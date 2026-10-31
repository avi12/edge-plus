import type { SwitchKey } from "#src/protocol.g.ts";

/** Segoe Fluent Icons code points: the icon of each switch's card. */
export const CARD_GLYPHS: Record<SwitchKey, number> = {
  nativeSplit: 0xF0E2,
  livePreview: 0xE890,
  theming: 0xE790,
  tabShape: 0xE7C3,
  webUiFollowsTheme: 0xE706,
  readingTheme: 0xE736,
  enhancedFind: 0xE721,
  protocolHandlers: 0xE71B,
  quietDevMode: 0xE7BA,
  startWithWindows: 0xE7E8,
  autoInject: 0xE945,
  autoUpdate: 0xE895
};

/** Segoe Fluent Icons code points of the icons that are not a switch's. */
export const GLYPHS = {
  injectReady: 0xE768,
  injected: 0xE73E,
  update: 0xE777,
  remove: 0xE711,
  grip: 0xE76F
} as const;
