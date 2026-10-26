import { winuiSvelte } from "../../src/vite/plugin.ts";
import { readFileSync } from "node:fs";
import path from "node:path";
import { defineConfig } from "vite";
import { z } from "zod";

// What the examples' module server reads: the plugin, and the package's own
// specifiers (winui-svelte/host, winui-svelte/css, ...) pointed at its
// sources, since inside the package they are not installed anywhere.

const packageRoot = path.join(import.meta.dirname, "..", "..");
const packageName = "winui-svelte";

const manifestSchema = z.object({ exports: z.record(z.string(), z.string()) });

function aliases(): Array<{
  find: RegExp;
  replacement: string;
}> {
  const parsedManifest = manifestSchema.safeParse(JSON.parse(readFileSync(path.join(packageRoot, "package.json"), "utf8")));
  if (!parsedManifest.success) {
    return [];
  }

  return Object.entries(parsedManifest.data.exports).map(([subpath, file]) => ({
    find: new RegExp(`^${packageName}${subpath.slice(1)}$`, "u"),
    replacement: path.join(packageRoot, file)
  }));
}

export default defineConfig({
  plugins: [winuiSvelte()],
  resolve: {
    alias: aliases()
  }
});
