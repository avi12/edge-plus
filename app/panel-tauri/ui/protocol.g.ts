// Generated from src-tauri/src/ui/protocol.rs by its export test (cargo test).
// Do not edit: change protocol.rs and run the test again.

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

export type View = { productName: string, openEdgeHint: string | null, buildNotice: string, sections: Array<SectionView>, update: UpdateView | null, footer: FooterView, dropHint: string, };

export type ColorMode = "light" | "dark";

export type Action = { "kind": "setSwitch", key: SwitchKey, isOn: boolean, } | { "kind": "inject" } | { "kind": "restartToUpdate" } | { "kind": "addStoreTheme", url: string, } | { "kind": "addLocalTheme", path: string, } | { "kind": "removeTheme", id: string, } | { "kind": "reorderThemes", ids: Array<string>, } | { "kind": "moveTheme", id: string, offset: number, } | { "kind": "assignTheme", mode: ColorMode, id: string, };

export type Bounds = { x: number, y: number, width: number, height: number, };

export type CoreMessage = { "type": "place", bounds: Bounds, } | { "type": "view", view: View, } | { "type": "reply", id: number, error: string | null, } | { "type": "notice", text: string, } | { "type": "status", line: string, } | { "type": "bringBack" } | { "type": "close" };

export type UiMessage = { "type": "runtimeMissing" } | { "type": "refresh" } | { "type": "action", id: number, action: Action, } | { "type": "bounds", bounds: Bounds, };

