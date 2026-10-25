import type { ThemeRow } from "../protocol.g.ts";
import { mountControl } from "./mount.svelte.ts";
import ThemeRowView from "./theme-row.svelte";
import { Grid, projectAs } from "./winui.ts";
import { unboxObject } from "@microsoft/dynwinrt";

const rowKeySeparator = "\n";

// Keyed by everything a row shows, so an unchanged row is left alone (and
// does not re-animate).
export function rowKey(row: ThemeRow): string {
  return [row.id, row.name, row.badges.join(",")].join(rowKeySeparator);
}

export function idOf(key: string): string {
  return key.split(rowKeySeparator)[0];
}

export function nameOf(key: string): string {
  return key.split(rowKeySeparator)[1];
}

// A row key as a list holds it: a boxed string.
export function keyText(item: unknown): string {
  const value = unboxObject(item);
  return typeof value === "string" ? value : "";
}

// One theme's row (theme-row.svelte). `onRemove` is null for a theme
// installed in Edge.
export function themeRow(theme: ThemeRow, onRemove: (() => void) | null): Grid {
  return projectAs(mountControl(ThemeRowView, {
    theme,
    onRemove
  }).root, Grid);
}
