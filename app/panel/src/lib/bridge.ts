import { invoke } from "@tauri-apps/api/core";

// The shape of the state the Rust core returns, and typed wrappers over every
// Tauri command. One wrapper per command keeps the invoke-string names in a
// single place, matching src-tauri/src/commands.rs.

/** The core's event for a local theme that changed on disk; same name as `WATCH_EVENT` in watch.rs. */
export const THEME_REFRESHED_EVENT = "theme-refreshed";

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
}

export interface AppState {
  features: Features;
  theming: Theming;
  edgeRunning: boolean;
  splitLoaded: boolean;
}

export type Mode = "light" | "dark";

export function loadState(): Promise<AppState> {
  return invoke<AppState>("load_state");
}

export function setSplit(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_split", { enabled });
}

export function setTheming(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_theming", { enabled });
}

export function setQuietDevMode(enabled: boolean): Promise<AppState> {
  return invoke<AppState>("set_quiet_dev_mode", { enabled });
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
