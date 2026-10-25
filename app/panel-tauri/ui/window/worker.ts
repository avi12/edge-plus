import { roInitialize } from "@microsoft/dynwinrt";
import type { MessagePort } from "node:worker_threads";
import { workerData } from "node:worker_threads";

// The UI thread. Its apartment comes first, so everything loaded after it may
// create WinRT objects as it loads.
const singleThreadedApartment = 0;

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

roInitialize(singleThreadedApartment);
const ports: unknown = workerData;
if (isWindowPorts(ports)) {
  if (ports.modules === undefined) {
    const { runWindow } = await import("./session.ts");
    await runWindow(ports.link);
  } else {
    // Imported by name, so the window kit's bundle leaves Vite out.
    const hotWindow = "../dev/hot-window.ts";
    const { runHotWindow } = await import(hotWindow);
    await runHotWindow(ports.link, ports.modules);
  }
}

// WinUI has ended, but handles it left would keep this thread alive; in a
// worker this ends only the worker, which ends the process (main.ts).
process.exit(0);
