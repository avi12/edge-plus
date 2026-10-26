import { startModuleServer } from "../../src/vite/server.ts";
import { initWindowsAppSdk } from "../../src/window/bootstrap.ts";
import path from "node:path";
import { createInterface } from "node:readline";
import { parseArgs } from "node:util";
import { MessageChannel, Worker } from "node:worker_threads";

// Runs an example in a real WinUI window, on a worker (WinUI's loop never
// gives its thread back), its modules served by Vite.
//
//   node --disable-warning=ExperimentalWarning examples/harness/launch.ts <entry> [--name value ...]
//
// <entry> is a module of the package (examples/lists/main.ts) exporting
// `start(host)`. The first line on stdout is `pid <n>`; then every message the
// example reports, one JSON object per line. Each line on stdin is a command
// for the example. The process ends when the window closes or stdin does.

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;
const packageRoot = path.join(import.meta.dirname, "..", "..");

const { values, positionals } = parseArgs({
  allowPositionals: true,
  options: {
    rows: {
      type: "string"
    },
    backdrop: {
      type: "string"
    },
    view: {
      type: "string"
    }
  }
});
const entry = positionals[0];
if (entry === undefined) {
  throw new Error("name the example's entry module, relative to the package");
}

initWindowsAppSdk({
  major: windowsAppSdkMajor,
  minor: windowsAppSdkMinor,
  folders: [path.join(packageRoot, ".winapp", "bin", process.arch)]
});
process.stdout.write(`pid ${process.pid}\n`);

const modules = new MessageChannel();
const link = new MessageChannel();
const server = await startModuleServer({
  root: packageRoot,
  configFile: path.join(import.meta.dirname, "vite.config.ts"),
  port: modules.port1
});
const worker = new Worker(new URL("worker.ts", import.meta.url), {
  workerData: {
    modules: modules.port2,
    link: link.port2,
    entry: `/${entry}`,
    options: values
  },
  transferList: [modules.port2, link.port2]
});
link.port1.on("message", message => {
  process.stdout.write(`${JSON.stringify(message)}\n`);
});
// A byte order mark opens the first line from a Windows PowerShell host.
const byteOrderMark = /^﻿/u;
createInterface({ input: process.stdin }).on("line", line => {
  const command = line.replace(byteOrderMark, "").trim();
  if (command.length > 0) {
    link.port1.postMessage(JSON.parse(command));
  }
}).on("close", () => {
  link.port1.postMessage({ cmd: "close" });
});
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", async code => {
  await server.close();
  process.exit(process.exitCode ?? code);
});
