import { createFakeHost, OFF_SCREEN_BOUNDS, savedView } from "./fake-host.ts";
import type { View } from "#src/protocol.g.ts";
import { CoreMessageType } from "#src/protocol.g.ts";
import { type ChildProcess, spawn, spawnSync } from "node:child_process";
import path from "node:path";
import { createInterface } from "node:readline";
import type { Json } from "winui-svelte/state/host";

// A stand-in for the Rust core, to work on the window alone and to test it:
// it starts the window as the core does (stdin and stdout piped, one frame per
// line) and speaks to it through a fake host (fake-host.ts). EDGEPLUS_WINDOW_KIT
// runs a built window kit (dist/window) in place of the sources.

const APP_ROOT = path.join(import.meta.dirname, "..", "..");
const EXIT_WAIT_MILLISECONDS = 8000;
const KIT_VARIABLE = "EDGEPLUS_WINDOW_KIT";
const KIT = process.env[KIT_VARIABLE];

/** The core's side of a running window. */
interface FakeCore extends ReturnType<typeof createFakeHost> {
  readonly child: ChildProcess;
  /** Asks the window to close, and ends its process tree if it has not exited in time. */
  stop(): Promise<number | null>;
}

/**
 * Starts the window. `entry` is the script to run (main.ts of the checkout, or
 * a window kit's main.mjs with its own node.exe).
 */
export function startFakeCore({ view = savedView(), isBackground = true, theme = "system", refuse = () => null, node = KIT === undefined ? process.execPath : path.join(KIT, "node.exe"), entry = KIT === undefined ? path.join(APP_ROOT, "ui", "main.ts") : path.join(KIT, "main.mjs") }: {
  view?: View;
  theme?: string;
  refuse?: (action: Json) => string | null;
  isBackground?: boolean;
  node?: string;
  entry?: string;
} = {}) {
  const child = spawn(node, ["--disable-warning=ExperimentalWarning", entry], {
    stdio: ["pipe", "pipe", "inherit"],
    env: {
      ...process.env,
      EDGEPLUS_WINDOW_BACKGROUND: isBackground ? "1" : "0",
      EDGEPLUS_WINDOW_THEME: theme
    },
    windowsHide: true
  });
  const exit = new Promise<number | null>(resolve => child.once("exit", code => resolve(code)));
  const host = createFakeHost({
    view,
    refuse
  });
  host.attach(line => child.stdin?.write(`${line}\n`));
  createInterface({ input: child.stdout }).on("line", line => host.receive(line));

  if (isBackground) {
    host.request(CoreMessageType.place, OFF_SCREEN_BOUNDS);
  }

  host.pushView();
  const core: FakeCore = Object.assign(host, {
    child,
    async stop() {
      host.request(CoreMessageType.close);
      const timer = setTimeout(() => {
        spawnSync("taskkill", ["/PID", String(child.pid), "/T", "/F"]);
      }, EXIT_WAIT_MILLISECONDS);
      const code = await exit;
      clearTimeout(timer);
      return code;
    }
  });
  return core;
}
