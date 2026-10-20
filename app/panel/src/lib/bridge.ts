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
  EnhancedFind = "enhancedFind",
  WebUiFollowsTheme = "webUiFollowsTheme",
  LivePreview = "livePreview",
  TabSpine = "tabSpine"
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

/** Where a theme came from; the values match `ThemeSource` in settings.rs. */
export enum ThemeSource {
  Store = "store",
  Local = "local"
}

export interface ThemeEntry {
  id: string;
  name: string;
  source: ThemeSource;
  origin: string;
}

/** A theme Edge already has installed, read from its profiles; matches `InstalledTheme` in installed_themes.rs. */
export interface EdgeTheme {
  id: string;
  name: string;
  /** Edge is showing it right now. */
  isActive: boolean;
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
  enhancedFind: boolean;
  webUiFollowsTheme: boolean;
  livePreview: boolean;
  tabSpine: boolean;
}

export interface AppState {
  features: Features;
  theming: Theming;
  /** Themes installed in Edge that the panel did not add. */
  edgeThemes: EdgeTheme[];
  edgeRunning: boolean;
  /** Windows starts the panel at sign-in; read from the registry, not from settings. */
  startWithWindows: boolean;
  splitLoaded: boolean;
  /** The `msedge.dll` build running Edge is on; null when Edge is not running. */
  edgeBuild: string | null;
  /** The only build the split DLL supports. */
  supportedEdgeBuild: string;
  /** The features that cannot run on the running build. */
  unsupported: Feature[];
}

/** The Windows colour mode a theme is picked for; the values match `Mode` in settings.rs. */
export enum Mode {
  Light = "light",
  Dark = "dark"
}

/** Every Tauri command the core exposes; the values match the handler names in commands.rs. */
enum Command {
  LoadState = "load_state",
  TakeEdgeUpdate = "take_edge_update",
  SetSplit = "set_split",
  SetTheming = "set_theming",
  SetQuietDevMode = "set_quiet_dev_mode",
  SetStartWithWindows = "set_start_with_windows",
  SetProtocolHandlers = "set_protocol_handlers",
  SetEnhancedFind = "set_enhanced_find",
  SetWebUiFollowsTheme = "set_web_ui_follows_theme",
  SetLivePreview = "set_live_preview",
  SetTabSpine = "set_tab_spine",
  AddStoreTheme = "add_store_theme",
  AddLocalTheme = "add_local_theme",
  RemoveTheme = "remove_theme",
  SetModeTheme = "set_mode_theme"
}

export function loadState(): Promise<AppState> {
  return invoke<AppState>(Command.LoadState);
}

/** The Edge update that switched features off before the UI was listening, handed over once. */
export function takeEdgeUpdate(): Promise<EdgeUpdate | null> {
  return invoke<EdgeUpdate | null>(Command.TakeEdgeUpdate);
}

export function setSplit(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetSplit, { enabled });
}

export function setTheming(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetTheming, { enabled });
}

export function setQuietDevMode(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetQuietDevMode, { enabled });
}

export function setStartWithWindows(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetStartWithWindows, { enabled });
}

export function setProtocolHandlers(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetProtocolHandlers, { enabled });
}

export function setEnhancedFind(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetEnhancedFind, { enabled });
}

export function setWebUiFollowsTheme(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetWebUiFollowsTheme, { enabled });
}

export function setLivePreview(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetLivePreview, { enabled });
}

export function setTabSpine(enabled: boolean): Promise<AppState> {
  return invoke<AppState>(Command.SetTabSpine, { enabled });
}

export function addStoreTheme(url: string): Promise<AppState> {
  return invoke<AppState>(Command.AddStoreTheme, { url });
}

export function addLocalTheme(path: string): Promise<AppState> {
  return invoke<AppState>(Command.AddLocalTheme, { path });
}

export function removeTheme(id: string): Promise<AppState> {
  return invoke<AppState>(Command.RemoveTheme, { id });
}

export function setModeTheme(mode: Mode, id: string): Promise<AppState> {
  return invoke<AppState>(Command.SetModeTheme, {
    mode,
    id
  });
}
