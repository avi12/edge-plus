import { MessagePort, workerData } from "node:worker_threads";
import { runOnUiThread } from "winui-svelte/thread";
import { z } from "zod";

// The UI thread (winui-svelte/thread): its apartment comes first, so
// everything loaded after it may create WinRT objects as it loads.

// What main.ts hands this thread: the link to the core and, run from source,
// the port to the dev server the window's modules come from.
const windowPortsSchema = z.object({
  link: z.instanceof(MessagePort),
  modules: z.instanceof(MessagePort).optional()
});

export type WindowPorts = z.infer<typeof windowPortsSchema>;

await runOnUiThread(async () => {
  const parsed = windowPortsSchema.safeParse(workerData);
  if (!parsed.success) {
    return;
  }

  const ports = parsed.data;
  if (ports.modules === undefined) {
    const { runSession } = await import("./host/session.ts");
    await runSession(ports.link);
  } else {
    // Imported by name, so the window kit's bundle leaves Vite out.
    const hotWindow = "../dev/hot-window.ts";
    const { runHotWindow } = await import(hotWindow);
    await runHotWindow(ports.link, ports.modules);
  }
});
