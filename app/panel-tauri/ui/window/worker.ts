import { roInitialize } from "@microsoft/dynwinrt";
import type { MessagePort } from "node:worker_threads";
import { workerData } from "node:worker_threads";

// The UI thread. Its apartment comes first, so everything loaded after it may
// create WinRT objects as it loads.
const singleThreadedApartment = 0;

roInitialize(singleThreadedApartment);
const { runWindow } = await import("./session.ts");
const port: unknown = workerData;
if (isPort(port)) {
  await runWindow(port);
}

// WinUI has ended, but handles it left would keep this thread alive; in a
// worker this ends only the worker, which ends the process (main.ts).
process.exit(0);

function isPort(value: unknown): value is MessagePort {
  return typeof value === "object" && value !== null && "postMessage" in value;
}
