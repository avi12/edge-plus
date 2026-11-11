// Generated from src-tauri/src/ui/protocol.rs by its export test (cargo test).
// Do not edit: change protocol.rs and run the test again.

import type { ProtocolWithReturn } from "winui-svelte/messaging";

export type Feature = "nativeSplit" | "theming" | "quietDevMode" | "protocolHandlers" | "enhancedFind" | "webUiFollowsTheme" | "livePreview" | "cyberpunkSkin" | "readingTheme";

export type ThemeBadge = "store" | "local" | "inUse" | "installedInEdge" | "light" | "dark";

export type ThemeRow = { id: string, name: string, badges: Array<ThemeBadge>, 
/**
 * The store URL or local path a theme was added from; empty for a theme
 * that only Edge knows.
 */
origin: string, };

export type ThemeOption = { id: string, name: string, };

export type ThemingView = { options: Array<ThemeOption>, light: string, dark: string, 
/**
 * Edge's default theme, the fixed first row: its id is empty and it has
 * no source badge.
 */
defaultTheme: ThemeRow, 
/**
 * Every theme in display order: a panel-owned one has the `local` or
 * `store` badge, an Edge-only one `installedInEdge`.
 */
themes: Array<ThemeRow>, };

export type SwitchKey = "nativeSplit" | "theming" | "quietDevMode" | "protocolHandlers" | "enhancedFind" | "webUiFollowsTheme" | "livePreview" | "cyberpunkSkin" | "readingTheme" | "startWithWindows" | "autoInject" | "autoUpdate";

export type EdgeBuild = { version: string, isUpdated: boolean, };

export type CardHold = { "kind": "paused", edge: EdgeBuild, } | { "kind": "unavailable", edge: EdgeBuild, } | { "kind": "needs", feature: Feature, };

export type CardView = { key: SwitchKey, isOn: boolean, isLocked: boolean, hold: CardHold | null, };

export type InjectOffer = "hidden" | "ready" | "injected";

export type UpdateView = { version: string, };

export type FooterView = { author: string, authorUri: string, version: string, };

export type View = { productName: string, 
/**
 * The id the core registered (name and icon) for its toasts; the window
 * takes it too, so the taskbar shows it as Edge+.
 */
appUserModelId: string, 
/**
 * The icon file the core wrote, for the title bar and the taskbar.
 */
icon: string, isEdgeRunning: boolean, 
/**
 * None when the build cannot be read.
 */
edge: EdgeBuild | null, 
/**
 * The features this build pauses.
 */
paused: Array<Feature>, cards: Array<CardView>, theming: ThemingView, inject: InjectOffer, update: UpdateView | null, footer: FooterView, };

export type ColorMode = "light" | "dark";

export type Action = { "kind": "setSwitch", key: SwitchKey, isOn: boolean, } | { "kind": "inject" } | { "kind": "restartToUpdate" } | { "kind": "addStoreTheme", url: string, shouldUseNow: boolean, } | { "kind": "addLocalTheme", path: string, shouldUseNow: boolean, } | { "kind": "removeTheme", id: string, } | { "kind": "removeEdgeTheme", id: string, } | { "kind": "reorderThemes", ids: Array<string>, } | { "kind": "moveTheme", id: string, offset: number, } | { "kind": "assignTheme", mode: ColorMode, id: string, };

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
