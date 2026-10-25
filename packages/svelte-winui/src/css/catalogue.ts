import type { ResourceCatalogue } from "./resources.ts";
import { createCatalogue } from "./resources.ts";
import { readFileSync } from "node:fs";

// The catalogue of WinUI's own resources, from winui-resources.g.json (which
// scripts/css/export-winui-resources.ts writes). Node only: it runs where the
// Vite plugin and the tests do, never in the window.

const catalogueFile = new URL("./winui-resources.g.json", import.meta.url);

function isStringList(value: unknown): value is string[] {
  return Array.isArray(value) && value.every(item => typeof item === "string");
}

export function loadWinuiCatalogue(): ResourceCatalogue {
  const parsed: unknown = JSON.parse(readFileSync(catalogueFile, "utf8"));
  const isCatalogue = typeof parsed === "object" && parsed !== null;
  if (!isCatalogue) {
    throw new TypeError(`${catalogueFile.pathname} is not a resource catalogue`);
  }

  const groups: Record<string, string[]> = {};
  for (const [kind, keys] of Object.entries(parsed)) {
    if (isStringList(keys)) {
      groups[kind] = keys;
    }
  }

  return createCatalogue(groups);
}
