import path from "node:path";
import type { MessagePort } from "node:worker_threads";
import { startModuleServer } from "winui-svelte/vite";

// Run from source, the window's modules come from a Vite dev server on
// main.ts's thread, so a saved .svelte file updates the open window in place.

const APP_ROOT = path.join(import.meta.dirname, "..", "..");

/** Serves the window's modules to the UI thread's module runner over `port`. */
export function serveWindowModules(port: MessagePort) {
  return startModuleServer({
    root: APP_ROOT,
    configFile: path.join(APP_ROOT, "vite.config.ts"),
    port
  });
}
