import { startModuleServer } from "../src/vite/server.ts";
import { initWinappsdk } from "@microsoft/dynwinrt";
import path from "node:path";
import { MessageChannel, Worker } from "node:worker_threads";

// Mounts every generated component in a real WinUI window, run in the
// background: off screen and never activated. It prints one JSON line, the
// report of test/mount-all.svelte.ts, and exits 1 when anything failed.
//
// The window's thread cannot take imports once WinUI runs, so the worker
// (mount-all.worker.ts) loads every module from this thread's Vite server
// first.

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;
const packageRoot = path.join(import.meta.dirname, "..");
const bindingsFolder = ".winapp/bindings/";
const bindingsSite = "generated/bindings.ts";

process.env["WINAPPSDK_BOOTSTRAP_DLL_PATH"] ??= path.join(
  packageRoot,
  ".winapp",
  "bin",
  process.arch,
  "Microsoft.WindowsAppRuntime.Bootstrap.dll"
);
initWinappsdk(windowsAppSdkMajor, windowsAppSdkMinor);

const modules = new MessageChannel();
const server = await startModuleServer({
  root: packageRoot,
  port: modules.port1,
  // The generated bindings are CommonJS that Node loads itself.
  isNative(file) {
    const normalized = file.replaceAll("\\", "/");
    return normalized.includes(bindingsFolder) || normalized.endsWith(bindingsSite);
  }
});
const worker = new Worker(new URL("mount-all.worker.ts", import.meta.url), {
  workerData: {
    modules: modules.port2
  },
  transferList: [modules.port2]
});
let report: unknown = null;
worker.on("message", message => {
  report = message;
});
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", async code => {
  await server.close();
  process.stdout.write(`${JSON.stringify(report)}\n`);
  process.exit(process.exitCode ?? code);
});
