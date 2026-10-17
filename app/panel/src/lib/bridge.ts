import { invoke } from "@tauri-apps/api/core";

// The shape of the state the Rust core returns, and typed wrappers over every
// Tauri command. One wrapper per command keeps the invoke-string names in a
// single place, matching src-tauri/src/commands.rs.

/** The core's event for a local theme that changed on disk; same name as `WATCH_EVENT` in watch.rs. */
export const THEME_REFRESHED_EVENT = "theme-refreshed";

/**
 * The core's event for a newly seen Edge build and what it switched off; same name as
 * `EDGE_UPDATED_EVENT` in edge_watch.rs.
 */
export const EDGE_UPDATED_EVENT = "edge-updated";

/** One feature the panel owns; the values match `Feature` in settings.rs. */
export enum Feature {
  NativeSplit = "nativeSplit",
  Theming = "theming",
  QuietDevMode = "quietDevMode",
  ProtocolHandlers = "protocolHandlers",
  Assistant = "assistant"
}

/** The browser assistant the chat button opens; matches `Assistant` in settings.rs. */
export enum Assistant {
  Copilot = "copilot",
  Gemini = "gemini"
}

/** A newly seen Edge build and what it switched off (often nothing); matches `BuildChange` in edge_build.rs. */
export interface EdgeUpdate {
  edgeBuild: string;
  previousBuild: string | null;
  supportedBuild: string;
  disabled: Feature[];
}

export interface ThemeRefresh {
  id: string;
  /** Why the edit was not applied; null when it was. */
  error: string | null;
}

/** What a local theme is picked from: a `.crx` / `.zip` file, or an unpacked folder. */
export enum LocalThemeKind {
  File = "file",
  Folder = "folder"
}

export type ThemeSource = "store" | "local";

export interface ThemeEntry {
  id: string;
  name: string;
  source: ThemeSource;
  origin: string;
}

export interface Theming {
  themes: ThemeEntry[];
  light: string;
  dark: string;
}

export interface Features {
  nativeSplit: boolean;
  theming: boolean;
  quietDevMode: boolean;
  protocolHandlers: boolean;
  assistant: Assistant;
}

export interface AppState {
  features: Features;
  theming: Theming;
  edgeRunning: boolean;
  splitLoaded: boolean;
  /** The `msedge.dll` build running Edge is on; null when Edge is not running. */
  edgeBuild: string | null;
  /** The only build the split DLL supports. */
  supportedEdgeBuild: string;
  /** The features that cannot run on the running build. */
  unsupported: Feature[];
}

export type Mode = "light" | "dark";

export function loadState(): Promise<AppState> {
  return invoke<AppState>("load_state");
}

/** The Edge update that switched features off before the UI was listening, handed over once. */
export function takeEdgeUpdate(): Promise<EdgeUpdate | null> {
  return invoke<EdgeUpdate | null>("take_edge_update");
}

export function setSplit(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_split", { enabled });
}

export function setTheming(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_theming", { enabled });
}

export function setAssistant(assistant: Assistant): Promise<AppState> {
  return invoke<AppState>("set_assistant", { assistant });
}

export function setQuietDevMode(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_quiet_dev_mode", { enabled });
}

export function setProtocolHandlers(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_protocol_handlers", { enabled });
}

export function addStoreTheme(url: string): Promise<AppState> {
  return invoke<AppState>("add_store_theme", { url });
}

export function addLocalTheme(path: string): Promise<AppState> {
  return invoke<AppState>("add_local_theme", { path });
}

export function removeTheme(id: string): Promise<AppState> {
  return invoke<AppState>("remove_theme", { id });
}

export function setModeTheme(mode: Mode, id: string): Promise<AppState> {
  return invoke<AppState>("set_mode_theme", {
    mode,
    id
  });
}
