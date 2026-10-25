import path from "node:path";
import { MessageChannel, Worker } from "node:worker_threads";
import { initWindowsAppSdk } from "svelte-winui/bootstrap";
import { startModuleServer } from "svelte-winui/vite";

// The host: it starts the Windows App SDK, serves the window's modules from a
// Vite dev server (so a saved .svelte file updates the open window) and starts
// the window, which runs on a worker because WinUI keeps its thread.
//
//   node main.ts        (from the folder `svelte-winui setup` was run in)

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;

initWindowsAppSdk({
  major: windowsAppSdkMajor,
  minor: windowsAppSdkMinor,
  folders: [path.join(process.cwd(), ".winapp", "bin", process.arch)]
});

const modules = new MessageChannel();
await startModuleServer({
  root: import.meta.dirname,
  configFile: path.join(import.meta.dirname, "vite.config.ts"),
  port: modules.port1
});
const worker = new Worker(new URL("./window.ts", import.meta.url), {
  workerData: {
    modules: modules.port2
  },
  transferList: [modules.port2]
});
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", code => {
  process.exit(process.exitCode ?? code);
});
