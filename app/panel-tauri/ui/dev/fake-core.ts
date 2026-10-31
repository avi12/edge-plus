import type { Bounds, View } from "#src/protocol.g.ts";
import { CoreMessageType } from "#src/protocol.g.ts";
import { type ChildProcess, spawn, spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline";
import type { Json } from "winui-svelte/state/host";
import { z } from "zod";

// A stand-in for the Rust core, to work on the window alone and to test it:
// it starts the window as the core does (stdin and stdout piped, one frame per
// line), sends its View, answers every request of the window's, applies a
// switch to the View the way the core does (a new View before the answer) and
// records what the window sent. EDGEPLUS_WINDOW_KIT runs a built window kit
// (dist/window) in place of the sources.

const APP_ROOT = path.join(import.meta.dirname, "..", "..");
const SAVED_VIEW_FILE = path.join(import.meta.dirname, "fixture-view.json");
const ICON_FILE = path.join(APP_ROOT, "src-tauri", "icons", "icon.ico");
const OFF_SCREEN_BOUNDS: Bounds = {
  x: -8000,
  y: 0,
  width: 560,
  height: 1700
};
const EXIT_WAIT_MILLISECONDS = 8000;
const KIT_VARIABLE = "EDGEPLUS_WINDOW_KIT";
const KIT = process.env[KIT_VARIABLE];

const setSwitchSchema = z.object({
  kind: z.literal("setSwitch"),
  key: z.string(),
  isOn: z.boolean()
});
const actionSchema = z.object({
  type: z.literal("action"),
  data: z.json()
});
const windowFrameSchema = z.object({
  id: z.number(),
  type: z.string(),
  data: z.json().optional()
});

/** What the window sent the core, in order. */
interface WindowMessage {
  type: string;
  data: Json | undefined;
}

/** The core's side of a running window. */
interface FakeCore {
  readonly child: ChildProcess;
  readonly messages: WindowMessage[];
  readonly actions: Json[];
  view: View;
  /** Sends the current View to the window. */
  pushView(): void;
  /** Sends the window a request of the core's own. */
  request(type: string, data?: Json): void;
  /** Resolves when the window sent a message of this type (one already received counts). */
  waitFor(type: string): Promise<WindowMessage>;
  /** Asks the window to close, and ends its process tree if it has not exited in time. */
  stop(): Promise<number | null>;
}

/** The View the dev fixture saved, with the icon of this checkout. */
function savedView() {
  const saved: View = JSON.parse(readFileSync(SAVED_VIEW_FILE, "utf8"));
  return {
    ...saved,
    icon: ICON_FILE
  };
}

function applySwitch({ view, key, isOn }: {
  view: View;
  key: string;
  isOn: boolean;
}) {
  const card = view.cards.find(candidate => candidate.key === key);
  const hasCard = card !== undefined;
  if (hasCard) {
    card.isOn = isOn;
  }
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
  const messages: WindowMessage[] = [];
  const waiters = new Map<string, Array<(message: WindowMessage) => void>>();
  let nextIdentifier = 0;
  const exit = new Promise<number | null>(resolve => child.once("exit", code => resolve(code)));

  function send(frame: object) {
    child.stdin?.write(`${JSON.stringify(frame)}\n`);
  }

  const core: FakeCore = {
    child,
    messages,
    actions: [],
    view,
    pushView() {
      core.request(CoreMessageType.view, core.view);
    },
    request(type, data) {
      nextIdentifier += 1;
      send({
        id: nextIdentifier,
        type,
        data,
        timestamp: Date.now()
      });
    },
    waitFor(type) {
      const received = messages.find(message => message.type === type);
      const hasReceived = received !== undefined;
      if (hasReceived) {
        return Promise.resolve(received);
      }

      return new Promise(resolve => {
        waiters.set(type, [...waiters.get(type) ?? [], resolve]);
      });
    },
    async stop() {
      core.request(CoreMessageType.close);
      const timer = setTimeout(() => {
        spawnSync("taskkill", ["/PID", String(child.pid), "/T", "/F"]);
      }, EXIT_WAIT_MILLISECONDS);
      const code = await exit;
      clearTimeout(timer);
      return code;
    }
  };

  createInterface({ input: child.stdout }).on("line", line => {
    const frame = windowFrameSchema.safeParse(JSON.parse(line));
    if (!frame.success) {
      return;
    }

    const message = {
      type: frame.data.type,
      data: frame.data.data
    };
    messages.push(message);
    for (const waiter of waiters.get(message.type) ?? []) {
      waiter(message);
    }

    const action = actionSchema.safeParse(frame.data);
    const refusal = action.success ? refuse(action.data.data) : null;
    if (action.success) {
      core.actions.push(action.data.data);
      const setSwitch = setSwitchSchema.safeParse(action.data.data);
      const isApplied = setSwitch.success && refusal === null;
      if (isApplied) {
        applySwitch({
          view: core.view,
          key: setSwitch.data.key,
          isOn: setSwitch.data.isOn
        });
        core.pushView();
      }
    }

    send(
      refusal === null
        ? {
          id: frame.data.id,
          response: null
        }
        : {
          id: frame.data.id,
          error: refusal
        }
    );
  });

  if (isBackground) {
    core.request(CoreMessageType.place, OFF_SCREEN_BOUNDS);
  }

  core.pushView();
  return core;
}
