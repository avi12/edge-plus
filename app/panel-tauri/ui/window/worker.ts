import type { MessagePort } from "node:worker_threads";
import { workerData } from "node:worker_threads";
import { runOnUiThread } from "svelte-winui/thread";

// The UI thread (svelte-winui/thread): its apartment comes first, so
// everything loaded after it may create WinRT objects as it loads.

// What main.ts hands this thread: the link to the core and, run from source,
// the port to the dev server the window's modules come from.
export interface WindowPorts {
  link: MessagePort;
  modules?: MessagePort;
}

function isPort(value: unknown): value is MessagePort {
  return typeof value === "object" && value !== null && "postMessage" in value;
}

function isWindowPorts(value: unknown): value is WindowPorts {
  return typeof value === "object" && value !== null && "link" in value && isPort(value.link);
}

await runOnUiThread(async () => {
  const ports: unknown = workerData;
  if (!isWindowPorts(ports)) {
    return;
  }

  if (ports.modules === undefined) {
    const { runSession } = await import("./session.ts");
    await runSession(ports.link);
  } else {
    // Imported by name, so the window kit's bundle leaves Vite out.
    const hotWindow = "../dev/hot-window.ts";
    const { runHotWindow } = await import(hotWindow);
    await runHotWindow(ports.link, ports.modules);
  }
});
