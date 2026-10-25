import { createWindowModuleRunner } from "../src/vite/runner.ts";
import { roInitialize } from "@microsoft/dynwinrt";
import type { MessagePort } from "node:worker_threads";
import { parentPort, workerData } from "node:worker_threads";

// The UI thread of the mount test: its apartment first, then every module the
// test needs through the dev server, and only then WinUI, which keeps the
// thread for the rest of the run.

const singleThreadedApartment = 0;

function isModulesPort(value: unknown): value is { modules: MessagePort } {
  return typeof value === "object" && value !== null && "modules" in value;
}

interface Harness {
  run(report: (result: unknown) => void): Promise<void>;
}

function isHarness(value: unknown): value is Harness {
  return typeof value === "object" && value !== null && "run" in value && typeof value.run === "function";
}

roInitialize(singleThreadedApartment);
const data: unknown = workerData;
if (!isModulesPort(data)) {
  throw new Error("the worker needs the module server's port");
}

const { runner } = createWindowModuleRunner(data.modules);
const harness: unknown = await runner.import("/test/mount-all.svelte.ts");
if (!isHarness(harness)) {
  throw new Error("mount-all.svelte.ts exports no run()");
}

await harness.run(result => parentPort?.postMessage(result));
process.exit(0);
