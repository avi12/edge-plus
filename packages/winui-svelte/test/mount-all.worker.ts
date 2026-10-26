import { createWindowModuleRunner } from "../src/vite/runner.ts";
import type { Report } from "./mount-report.ts";
import { roInitialize } from "@microsoft/dynwinrt";
import { MessagePort, parentPort, workerData } from "node:worker_threads";
import { z } from "zod";

// The UI thread of the mount test: its apartment first, then every module the
// test needs through the dev server, and only then WinUI, which keeps the
// thread for the rest of the run.

const singleThreadedApartment = 0;

const workerDataSchema = z.object({ modules: z.instanceof(MessagePort) });

interface Harness {
  run(report: (result: Report) => void): Promise<void>;
}

roInitialize(singleThreadedApartment);
const data = workerDataSchema.parse(workerData);

const { runner } = createWindowModuleRunner(data.modules);
const harness = await runner.import<Harness>("/test/mount-all.svelte.ts");
const isHarness = typeof harness.run === "function";
if (!isHarness) {
  throw new Error("mount-all.svelte.ts exports no run()");
}

await harness.run(result => parentPort?.postMessage(result));
process.exit(0);
