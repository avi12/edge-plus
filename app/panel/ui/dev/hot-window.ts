import type { MessagePort } from "node:worker_threads";

import { createWindowModuleRunner } from "winui-svelte/runner";

import type * as RunSession from "#src/window/host/run-session.ts";

// The window run from source: its modules load through the dev server
// (window-modules.ts), which pushes its hot updates over the port.

/** Runs the session with its modules loaded through the dev server on `modules`. */
export async function runHotWindow({ link, modules }: { link: MessagePort; modules: MessagePort }) {
  const { runner } = createWindowModuleRunner(modules);
  const { runSession } = await runner.import<typeof RunSession>("/ui/window/host/run-session.ts");
  await runSession({ link });
}
