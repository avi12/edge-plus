import type { UiMessage } from "./protocol.g.ts";
import { initWinappsdk } from "@microsoft/dynwinrt";
import { existsSync } from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline";
import { MessageChannel, Worker } from "node:worker_threads";

// The window's process, started by the core with its stdin and stdout as the
// link: one JSON message per line each way (protocol.g.ts). The window itself
// runs on a worker, because WinUI's loop never gives its thread back to Node;
// this thread only relays lines.

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

function say(message: UiMessage): void {
  process.stdout.write(`${JSON.stringify(message)}\n`);
}

process.env[bootstrapVariable] ??= bootstrapPath();
try {
  initWinappsdk(windowsAppSdkMajor, windowsAppSdkMinor);
} catch (error) {
  // The core installs the runtime it carries and opens the window again.
  process.stderr.write(`${String(error)}\n`);
  say({ type: "runtimeMissing" });
  process.exit(1);
}

const channel = new MessageChannel();
// The same layout run from source (.ts) and from the bundled window kit (.mjs).
const worker = new Worker(new URL(`./window/worker${path.extname(import.meta.filename)}`, import.meta.url), {
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
channel.port1.on("message", (message: UiMessage) => say(message));
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", code => {
  process.exit(process.exitCode ?? code);
});
