import path from "node:path";
import { pathToFileURL } from "node:url";
import { MessagePort, workerData } from "node:worker_threads";

import { runOnUiThread } from "winui-svelte/thread";
import { z } from "zod";

// The UI thread (winui-svelte/thread): its apartment comes first, so
// everything loaded after it may create WinRT objects as it loads.

const windowPortsSchema = z.object({
  link: z.instanceof(MessagePort),
  modules: z.instanceof(MessagePort).optional()
});

/** What main.ts hands this thread: the link to the core and, run from source, the dev server's port. */
export type WindowPorts = z.infer<typeof windowPortsSchema>;

await runOnUiThread(async () => {
  const parsed = windowPortsSchema.safeParse(workerData);
  if (!parsed.success) {
    return;
  }

  const { link, modules } = parsed.data;
  const isBundled = modules === undefined;
  if (isBundled) {
    const { runSession } = await import("./host/run-session.ts");
    await runSession({ link });
    return;
  }

  const hotWindow = pathToFileURL(path.join(import.meta.dirname, "..", "dev", "hot-window.ts")).href;
  const { runHotWindow } = await import(hotWindow);
  await runHotWindow({
    link,
    modules
  });
});
