import { initWinappsdk } from "@microsoft/dynwinrt";
import { existsSync } from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline";
import { MessageChannel, Worker } from "node:worker_threads";

// The window's process, started by the core with its stdin and stdout as the
// link: one JSON message per line each way (protocol.ts). The window itself
// runs on a worker, because Application.start never returns its thread; this
// thread only relays lines.

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;
const bootstrapVariable = "WINAPPSDK_BOOTSTRAP_DLL_PATH";
const bootstrapName = "Microsoft.WindowsAppRuntime.Bootstrap.dll";

// `pnpm restore` puts the bootstrapper in .winapp; a packaged window carries
// it beside this file.
function bootstrapPath(): string {
  const beside = path.join(import.meta.dirname, bootstrapName);
  if (existsSync(beside)) {
    return beside;
  }
  return path.join(import.meta.dirname, "..", ".winapp", "bin", process.arch, bootstrapName);
}

process.env[bootstrapVariable] ??= bootstrapPath();
initWinappsdk(windowsAppSdkMajor, windowsAppSdkMinor);

const channel = new MessageChannel();
const worker = new Worker(new URL("./window/worker.ts", import.meta.url), {
  workerData: channel.port2,
  transferList: [channel.port2]
});

createInterface({ input: process.stdin }).on("line", line => {
  if (line.trim().length > 0) {
    channel.port1.postMessage(JSON.parse(line));
  }
}).on("close", () => {
  // The core is gone: so is its window.
  channel.port1.postMessage({ type: "close" });
});
channel.port1.on("message", message => {
  process.stdout.write(`${JSON.stringify(message)}\n`);
});
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", code => {
  process.exit(process.exitCode ?? code);
});
