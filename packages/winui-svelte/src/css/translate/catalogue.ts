import type { ResourceCatalogue } from "../runtime/resources.ts";
import { createCatalogue } from "../runtime/resources.ts";
import { readFileSync } from "node:fs";
import { z } from "zod";

// The catalogue of WinUI's own resources, from winui-resources.g.json (which
// scripts/css/export-winui-resources.ts writes). Node only: it runs where the
// Vite plugin and the tests do, never in the window.

const catalogueFile = new URL("../winui-resources.g.json", import.meta.url);

const stringListSchema = z.array(z.string());
const catalogueSchema = z.record(z.string(), z.unknown());

export function loadWinuiCatalogue(): ResourceCatalogue {
  const catalogue = catalogueSchema.safeParse(JSON.parse(readFileSync(catalogueFile, "utf8")));
  if (!catalogue.success) {
    throw new TypeError(`${catalogueFile.pathname} is not a resource catalogue`);
  }

  // A kind whose keys are not a list of strings is left out.
  const groups: Record<string, string[]> = {};
  for (const [kind, keys] of Object.entries(catalogue.data)) {
    const list = stringListSchema.safeParse(keys);
    if (list.success) {
      groups[kind] = list.data;
    }
  }

  return createCatalogue(groups);
}
