import { createWindowModuleRunner } from "../../src/vite/runner.ts";
import { runOnUiThread } from "../../src/window/thread.ts";
import type { RunExample } from "./run-example.ts";
import type { MessagePort } from "node:worker_threads";
import { workerData } from "node:worker_threads";

// The UI thread of an example: its apartment first, then every module the
// example needs through the module server (run-example.ts and what it
// imports; WinUI's loop takes the thread once the window runs).

const runExampleModule = "/examples/harness/run-example.ts";

interface Ports {
  modules: MessagePort;
  link: MessagePort;
  entry: string;
  options: Record<string, string>;
}

function isPort(value: unknown): value is MessagePort {
  return typeof value === "object" && value !== null && "postMessage" in value;
}

function isPorts(value: unknown): value is Ports {
  return typeof value === "object" && value !== null
    && "modules" in value && isPort(value.modules)
    && "link" in value && isPort(value.link)
    && "entry" in value && typeof value.entry === "string"
    && "options" in value && typeof value.options === "object";
}

await runOnUiThread(async () => {
  const data: unknown = workerData;
  if (!isPorts(data)) {
    throw new Error("the host passed no ports");
  }

  const { runner } = createWindowModuleRunner(data.modules);
  const { runExample } = await runner.import<{ runExample: RunExample }>(runExampleModule);
  await runExample(data.entry, data.link, data.options);
});
