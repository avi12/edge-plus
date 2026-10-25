import { format } from "node:util";
import type { MessagePort } from "node:worker_threads";
import { receiveMessageOnPort } from "node:worker_threads";
import type { HotPayload } from "vite";
import type { HMRLogger, ModuleRunnerTransport } from "vite/module-runner";
import { createNodeImportMeta, ESModulesEvaluator, ModuleRunner } from "vite/module-runner";

// The window thread's side of vite-server.ts: a module runner that loads the
// window's modules from the dev server and applies its hot updates.
//
// WinUI's message loop holds the window thread and never gives it back to
// Node, so once the window is up, no 'message' event arrives here: the app
// calls receive() from a timer of WinUI's own (a DispatcherQueue timer), on
// the UI thread, and every reply and update is handled there.

export interface WindowModuleRunner {
  readonly runner: ModuleRunner;
  // Takes in everything the server has sent.
  receive(): void;
}

// HMR's lines, sent for the server to print: this thread's own stderr is
// written by its event loop, which WinUI holds, so all but the first line
// would wait for the window to close. (Nor do they go to stdout, which is
// often the host's channel, as Edge+'s core reads it line by line.)
export const logEvent = "winui:log";
// An invoke that times out uses a Node timer, which never fires while WinUI
// holds the thread; a reply always comes.
const noTimeout = 0;
const fullReload = "full-reload";

function isHotPayload(value: unknown): value is HotPayload {
  return typeof value === "object" && value !== null && "type" in value;
}

function restartNotice(file: string | undefined): string {
  return `${file ?? "a module"} changed and no component takes it in place; restart the window to see it`;
}

export function createWindowModuleRunner(port: MessagePort): WindowModuleRunner {
  let handle: ((payload: HotPayload) => void) | null = null;

  function log(...messages: unknown[]): void {
    port.postMessage({
      type: "custom",
      event: logEvent,
      data: format(...messages)
    });
  }

  function deliver(message: unknown): void {
    if (!isHotPayload(message)) {
      return;
    }

    // A change no module accepts would rerun the window's entry, which
    // starts a second window: the developer restarts it instead.
    if (message.type === fullReload) {
      log(restartNotice(message.triggeredBy));
      return;
    }

    handle?.(message);
  }

  const logger: HMRLogger = {
    debug: log,
    error: log
  };
  const transport: ModuleRunnerTransport = {
    connect({ onMessage }) {
      handle = onMessage;
      // Until WinUI takes the thread, Node delivers messages itself.
      port.on("message", deliver);
    },
    send(payload) {
      port.postMessage(payload);
    },
    timeout: noTimeout
  };
  const runner = new ModuleRunner({
    transport,
    createImportMeta: createNodeImportMeta,
    hmr: {
      logger
    }
  }, new ESModulesEvaluator());
  return {
    runner,
    receive() {
      for (let received = receiveMessageOnPort(port); received !== undefined; received = receiveMessageOnPort(port)) {
        deliver(received.message);
      }
    }
  };
}
