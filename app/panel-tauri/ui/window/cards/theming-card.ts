import type { Action } from "../../protocol.g.ts";

// What the theme library asks of the window: the kinds of local theme it can
// pick, and how an action runs.

// A .crx or .zip file, or an unpacked folder.
export const LocalThemeKind = {
  File: "file",
  Folder: "folder"
} as const;
export type LocalThemeKind = (typeof LocalThemeKind)[keyof typeof LocalThemeKind];

// Runs an action in the core; resolves to why it failed, or null.
export type RunAction = (action: Action) => Promise<string | null>;
