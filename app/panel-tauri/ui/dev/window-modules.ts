import { startModuleServer } from "../svelte-winui/vite-server.ts";
import { rendererFor } from "../window/svelte-compile.ts";
import path from "node:path";
import type { MessagePort } from "node:worker_threads";
import type { ViteDevServer } from "vite";

// Run from source, the window's modules come from a Vite dev server on
// main.ts's thread, so a saved .svelte file updates the open window in place.

const sources = path.join(import.meta.dirname, "..");
// The one import site of the generated bindings, which are CommonJS that
// Node loads through package imports (#winui/*).
const bindingsImportSite = path.join(sources, "window", "winui.ts");

export function serveWindowModules(port: MessagePort): Promise<ViteDevServer> {
  return startModuleServer({
    root: sources,
    port,
    customRenderer: rendererFor,
    isNative: file => path.relative(file, bindingsImportSite) === ""
  });
}
