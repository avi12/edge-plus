import { svelte } from "@sveltejs/vite-plugin-svelte";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

// The dev-server port has ONE home: tauri.conf.json's `devUrl` (the port the
// packaged webview loads). Derive it here rather than repeating the literal, so
// the two can never drift.
const tauriConfig: {
  build: { devUrl: string };
} = JSON.parse(
  readFileSync(fileURLToPath(new URL("./src-tauri/tauri.conf.json", import.meta.url)), "utf8")
);
const devServerPort = Number(new URL(tauriConfig.build.devUrl).port);

// Tauri expects a fixed port and no clobbering of its own env vars.
export default defineConfig({
  plugins: [svelte()],
  // `@` → src, so internal modules import as `@/lib/…` not brittle `../../` chains.
  resolve: {
    alias: {
      "@": fileURLToPath(new URL("./src", import.meta.url))
    }
  },
  clearScreen: false,
  server: {
    port: devServerPort,
    strictPort: true,
    // src-tauri's locked .pdb/.exe artifacts crash the watcher (EBUSY on Windows).
    watch: {
      ignored: ["**/src-tauri/**"]
    }
  },
  build: {
    target: "esnext",
    sourcemap: false
  }
});
