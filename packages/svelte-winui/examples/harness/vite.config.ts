import { svelteWinui } from "../../src/vite/plugin.ts";
import { readFileSync } from "node:fs";
import path from "node:path";
import { defineConfig } from "vite";

// What the examples' module server reads: the plugin, and the package's own
// specifiers (svelte-winui/host, svelte-winui/css, ...) pointed at its
// sources, since inside the package they are not installed anywhere.

const packageRoot = path.join(import.meta.dirname, "..", "..");
const packageName = "svelte-winui";

function isExports(value: unknown): value is Record<string, string> {
  return typeof value === "object" && value !== null;
}

function aliases(): Array<{
  find: RegExp;
  replacement: string;
}> {
  const manifest: unknown = JSON.parse(readFileSync(path.join(packageRoot, "package.json"), "utf8"));
  const exported = typeof manifest === "object" && manifest !== null && "exports" in manifest ? manifest.exports : {};
  if (!isExports(exported)) {
    return [];
  }

  return Object.entries(exported).map(([subpath, file]) => ({
    find: new RegExp(`^${packageName}${subpath.slice(1)}$`, "u"),
    replacement: path.join(packageRoot, file)
  }));
}

export default defineConfig({
  plugins: [svelteWinui()],
  resolve: {
    alias: aliases()
  }
});
