import { CoreMessageType, UiMessageType } from "./protocol.g.ts";
import type { WindowPorts } from "./window/worker.ts";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { MessageChannel, Worker } from "node:worker_threads";
import { initWindowsAppSdk, WindowsAppRuntimeMissing } from "winui-svelte/bootstrap";
import type { Json } from "winui-svelte/state/host";
import { createStdioTransport, reserveStdout } from "winui-svelte/state/host";

// The window's process, started by the core with its stdin and stdout as the
// link: one JSON frame per line each way (protocol.g.ts). WinUI's loop owns
// the thread the window runs on, so this thread only relays the lines to the
// worker and speaks for the window where it cannot: no runtime to open it, and
// the core gone.

reserveStdout();

const WINDOWS_APP_SDK_MAJOR = 2;
const WINDOWS_APP_SDK_MINOR = 2;
const OWN_REQUEST_IDENTIFIER = 0;
const SOURCE_EXTENSION = ".ts";

const transport = createStdioTransport();

function ownRequest(type: string) {
  return {
    id: OWN_REQUEST_IDENTIFIER,
    type,
    timestamp: Date.now()
  };
}

try {
  initWindowsAppSdk({
    major: WINDOWS_APP_SDK_MAJOR,
    minor: WINDOWS_APP_SDK_MINOR,
    folders: [path.join(import.meta.dirname, "bin", process.arch)]
  });
} catch (error) {
  process.stderr.write(`${String(error)}\n`);

  const isRuntimeMissing = error instanceof WindowsAppRuntimeMissing;
  if (isRuntimeMissing) {
    transport.send(ownRequest(UiMessageType.runtimeMissing));
  }

  process.exit(1);
}

const link = new MessageChannel();
const EXTENSION = path.extname(import.meta.filename);
const IS_FROM_SOURCE = EXTENSION === SOURCE_EXTENSION;
const modules = IS_FROM_SOURCE ? new MessageChannel() : null;
const HAS_MODULE_SERVER = modules !== null;
if (HAS_MODULE_SERVER) {
  const windowModules = pathToFileURL(path.join(import.meta.dirname, "dev", "window-modules.ts")).href;
  const { serveWindowModules } = await import(windowModules);
  await serveWindowModules(modules.port1);
}

const workerPorts: WindowPorts = HAS_MODULE_SERVER
  ? {
    link: link.port2,
    modules: modules.port2
  }
  : { link: link.port2 };
const worker = new Worker(path.join(import.meta.dirname, "window", `worker${EXTENSION}`), {
  workerData: workerPorts,
  transferList: HAS_MODULE_SERVER ? [link.port2, modules.port2] : [link.port2]
});

transport.listen({
  message: (frame: Json) => link.port1.postMessage(frame),
  malformed: reason => process.stderr.write(`malformed line from the core: ${reason}\n`),
  closed: () => link.port1.postMessage(ownRequest(CoreMessageType.close))
});
link.port1.on("message", frame => transport.send(frame));
worker.on("error", error => {
  process.stderr.write(`${error.stack ?? String(error)}\n`);
  process.exitCode = 1;
});
worker.on("exit", code => {
  process.exit(process.exitCode ?? code);
});
