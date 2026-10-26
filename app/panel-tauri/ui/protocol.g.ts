// Generated from src-tauri/src/ui/protocol.rs by its export test (cargo test).
// Do not edit: change protocol.rs and run the test again.

import type { ProtocolWithReturn } from "winui-svelte/messaging";

export type CardWording = { title: string, description: string, 
/**
 * A Segoe Fluent Icons code point.
 */
glyph: number, };

export type ThemeRow = { id: string, name: string, badges: Array<string>, removeLabel: string, };

export type ThemeOption = { id: string, label: string, };

export type ThemingLabels = { storeLinkPlaceholder: string, add: string, addLocal: string, pickFile: string, pickFolder: string, whenLight: string, whenDark: string, themeListName: string, reorderHelp: string, };

export type ThemingView = { labels: ThemingLabels, options: Array<ThemeOption>, light: string, dark: string, own: Array<ThemeRow>, edge: Array<ThemeRow>, 
/**
 * Shown in place of both lists while there are no themes.
 */
empty: string | null, };

export type InjectView = { wording: CardWording, button: string, isEnabled: boolean, };

export type InjectSlot = { offer: InjectView | null, };

export type SwitchKey = "nativeSplit" | "theming" | "quietDevMode" | "protocolHandlers" | "enhancedFind" | "webUiFollowsTheme" | "livePreview" | "tabShape" | "readingTheme" | "startWithWindows" | "autoInject" | "autoUpdate";

export type CardView = { key: SwitchKey, wording: CardWording, isOn: boolean, isLocked: boolean, note: string, theming: ThemingView | null, inject: InjectSlot | null, };

export type SectionView = { name: string, cards: Array<CardView>, };

export type UpdateView = { wording: CardWording, button: string, restarting: string, };

export type FooterView = { by: string, author: string, authorUri: string, version: string, };

export type View = { productName: string, 
/**
 * The id the core registered (name and icon) for its toasts; the window
 * takes it too, so the taskbar shows it as Edge+.
 */
appUserModelId: string, 
/**
 * The icon file the core wrote, for the title bar and the taskbar.
 */
icon: string, openEdgeHint: string | null, buildNotice: string, sections: Array<SectionView>, update: UpdateView | null, footer: FooterView, dropHint: string, };

export type ColorMode = "light" | "dark";

export type Action = { "kind": "setSwitch", key: SwitchKey, isOn: boolean, } | { "kind": "inject" } | { "kind": "restartToUpdate" } | { "kind": "addStoreTheme", url: string, } | { "kind": "addLocalTheme", path: string, } | { "kind": "removeTheme", id: string, } | { "kind": "reorderThemes", ids: Array<string>, } | { "kind": "moveTheme", id: string, offset: number, } | { "kind": "assignTheme", mode: ColorMode, id: string, };

export type Bounds = { x: number, y: number, width: number, height: number, };

export const CoreMessageType = {
  place: "place",
  view: "view",
  notice: "notice",
  status: "status",
  bringBack: "bringBack",
  close: "close",
} as const;

export const UiMessageType = {
  runtimeMissing: "runtimeMissing",
  refresh: "refresh",
  action: "action",
  bounds: "bounds",
} as const;

export interface EdgePlusProtocolMap {
  [CoreMessageType.place]: ProtocolWithReturn<Bounds, void>;
  [CoreMessageType.view]: ProtocolWithReturn<View, void>;
  [CoreMessageType.notice]: ProtocolWithReturn<string, void>;
  [CoreMessageType.status]: ProtocolWithReturn<string, void>;
  [CoreMessageType.bringBack]: ProtocolWithReturn<void, void>;
  [CoreMessageType.close]: ProtocolWithReturn<void, void>;
  [UiMessageType.runtimeMissing]: ProtocolWithReturn<void, void>;
  [UiMessageType.refresh]: ProtocolWithReturn<void, void>;
  [UiMessageType.action]: ProtocolWithReturn<Action, void>;
  [UiMessageType.bounds]: ProtocolWithReturn<Bounds, void>;
}
