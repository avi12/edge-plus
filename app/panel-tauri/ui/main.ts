import { CoreMessageType, UiMessageType } from "./protocol.g.ts";
import type { WindowPorts } from "./window/worker.ts";
import path from "node:path";
import { createInterface } from "node:readline";
import { MessageChannel, Worker } from "node:worker_threads";
import { initWindowsAppSdk, WindowsAppRuntimeMissing } from "svelte-winui/bootstrap";
import type { RequestFrame, ResponseFrame } from "svelte-winui/messaging";

// The window's process, started by the core with its stdin and stdout as the
// link: one message frame per line each way (protocol.g.ts; the frames are
// svelte-winui/messaging's). The window itself runs on a worker, because
// WinUI's loop never gives its thread back to Node; this thread only relays
// lines, and speaks for the window where it cannot: no runtime to open it, and
// the core gone.

const windowsAppSdkMajor = 2;
const windowsAppSdkMinor = 2;

// A request frame of the window's own, which the core answers or ignores.
const ownRequestIdentifier = 0;

function request(type: string): RequestFrame {
  return {
    id: ownRequestIdentifier,
    type,
    timestamp: Date.now()
  };
}

function say(frame: RequestFrame | ResponseFrame): void {
  process.stdout.write(`${JSON.stringify(frame)}\n`);
}

try {
  // `pnpm restore` puts the bootstrapper in .winapp; a packaged window
  // carries it beside this file.
  initWindowsAppSdk({
    major: windowsAppSdkMajor,
    minor: windowsAppSdkMinor,
    folders: [import.meta.dirname, path.join(import.meta.dirname, "..", ".winapp", "bin", process.arch)]
  });
} catch (error) {
  // The core installs the runtime it carries and opens the window again.
  process.stderr.write(`${String(error)}\n`);

  if (error instanceof WindowsAppRuntimeMissing) {
    say(request(UiMessageType.runtimeMissing));
  }

  process.exit(1);
}

const channel = new MessageChannel();
const sourceExtension = ".ts";
// The same layout run from source (.ts) and from the bundled window kit (.mjs).
const extension = path.extname(import.meta.filename);
const isFromSource = extension === sourceExtension;
// Run from source, the window's modules come from a dev server on this
// thread, which updates the open window as its .svelte files are saved.
const modules = isFromSource ? new MessageChannel() : null;
if (modules !== null) {
  // Imported by name, so the window kit's bundle leaves Vite out.
  const windowModules = "./dev/window-modules.ts";
  const { serveWindowModules } = await import(windowModules);
  await serveWindowModules(modules.port1);
}

const workerPorts: WindowPorts = modules === null
  ? { link: channel.port2 }
  : {
    link: channel.port2,
    modules: modules.port2
  };
const worker = new Worker(new URL(`./window/worker${extension}`, import.meta.url), {
  workerData: workerPorts,
  transferList: modules === null ? [channel.port2] : [channel.port2, modules.port2]
});

createInterface({ input: process.stdin }).on("line", line => {
  if (line.trim().length > 0) {
    channel.port1.postMessage(JSON.parse(line));
  }
}).on("close", () => {
  // The core is gone: so is its window.
  channel.port1.postMessage(request(CoreMessageType.close));
});
channel.port1.on("message", (frame: RequestFrame | ResponseFrame) => say(frame));
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", code => {
  process.exit(process.exitCode ?? code);
});
