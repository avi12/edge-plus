import type { Bounds, View } from "#src/protocol.g.ts";
import { CoreMessageType } from "#src/protocol.g.ts";
import { readFileSync } from "node:fs";
import path from "node:path";
import type { Json } from "winui-svelte/state/host";
import { z } from "zod";

// The Rust core's side of the link, without Rust: it sends the saved View,
// answers every request of the window's, applies a switch to the View the way
// the core does (a new View before the answer) and records what the window
// sent. It speaks over lines it is attached to, so it serves a test that owns
// the window's process (fake-core.ts) and the dev server that keeps one up
// (dev-server.ts) alike.

const APP_ROOT = path.join(import.meta.dirname, "..", "..");
const SAVED_VIEW_FILE = path.join(import.meta.dirname, "fixture-view.json");
const ICON_FILE = path.join(APP_ROOT, "src-tauri", "icons", "icon.ico");
export const OFF_SCREEN_BOUNDS: Bounds = {
  x: -8000,
  y: 0,
  width: 560,
  height: 1700
};

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

/** The core's side of a link to a window. */
interface FakeHost {
  readonly messages: WindowMessage[];
  readonly actions: Json[];
  view: View;
  /** Where the lines for the window go; the host speaks only once attached. */
  attach(send: (line: string) => void): void;
  /** One line the window sent. */
  receive(line: string): void;
  /** Sends the current View to the window. */
  pushView(): void;
  /** Sends the window a request of the core's own. */
  request(type: string, data?: Json): void;
  /** Resolves when the window sent a message of this type (one already received counts). */
  waitFor(type: string): Promise<WindowMessage>;
}

/** The View the dev fixture saved, with the icon of this checkout. */
export function savedView() {
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

/** `refuse` returns a sentence to answer an action with an error, or null to accept it. */
export function createFakeHost({ view = savedView(), refuse = () => null }: {
  view?: View;
  refuse?: (action: Json) => string | null;
} = {}) {
  const messages: WindowMessage[] = [];
  const waiters = new Map<string, Array<(message: WindowMessage) => void>>();
  let nextIdentifier = 0;
  let sendLine: ((line: string) => void) | null = null;

  function send(frame: object) {
    sendLine?.(JSON.stringify(frame));
  }

  const host: FakeHost = {
    messages,
    actions: [],
    view,
    attach(write) {
      sendLine = write;
    },
    pushView() {
      host.request(CoreMessageType.view, host.view);
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
    receive(line) {
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
        host.actions.push(action.data.data);
        const setSwitch = setSwitchSchema.safeParse(action.data.data);
        const isApplied = setSwitch.success && refusal === null;
        if (isApplied) {
          applySwitch({
            view: host.view,
            key: setSwitch.data.key,
            isOn: setSwitch.data.isOn
          });
          host.pushView();
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
    }
  };
  return host;
}
