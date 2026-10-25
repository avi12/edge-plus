import type {
  Action,
  Bounds,
  CoreMessage,
  UiMessage,
  View
} from "../protocol.g.ts";
import { milliseconds } from "./layout.ts";
import { joinAppUserModel } from "./native.ts";
import { Shell } from "./shell.ts";
import { applyStyleClass, installStyles } from "./styles.ts";
import * as winui from "./winui.ts";
import { DispatcherQueue } from "./winui.ts";
import type { MessagePort } from "node:worker_threads";
import { receiveMessageOnPort } from "node:worker_threads";
import { registerControls, setClassHandler } from "svelte-winui";
import { exitApplication, runWindow } from "svelte-winui/window";

// WinUI's loop owns this thread until the window closes, so Node's event loop
// never turns here: the core's messages are taken off the port by a
// dispatcher timer, and every timer is the dispatcher's.
const inboxMilliseconds = 30;
// Set by the dev fixture for unattended captures: the window opens without
// taking focus (the fixture also places it off screen).
const backgroundVariable = "EDGEPLUS_WINDOW_BACKGROUND";

type MessageOfType = { [Message in CoreMessage as Message["type"]]: Message };
type MessageType = keyof MessageOfType;
type Handlers = { [Type in MessageType]: (message: MessageOfType[Type]) => void };

// The core's own process, so its messages are trusted to be well formed.
function isCoreMessage(value: unknown): value is CoreMessage {
  return typeof value === "object" && value !== null && "type" in value;
}

function dispatch<Type extends MessageType>(handlers: Handlers, message: MessageOfType[Type] & { type: Type }): void {
  handlers[message.type](message);
}

class Session {
  private readonly port: MessagePort;
  private readonly dispatcher = DispatcherQueue.getForCurrentThread();
  // Held here, as is every pending timer: a timer whose wrapper is collected
  // stops.
  private readonly inbox = this.dispatcher.createTimer();
  private readonly timers = new Set<ReturnType<DispatcherQueue["createTimer"]>>();
  private readonly replies = new Map<number, (error: string | null) => void>();
  private nextId = 1;
  private shell: Shell | null = null;
  private placement: Bounds | null = null;

  constructor(port: MessagePort) {
    this.port = port;
  }

  start(): void {
    this.inbox.interval = milliseconds(inboxMilliseconds);
    this.inbox.isRepeating = true;
    this.inbox.onTick(() => this.drain());
    this.inbox.start();
  }

  private drain(): void {
    let received = receiveMessageOnPort(this.port);
    while (received !== undefined) {
      const message: unknown = received.message;
      if (isCoreMessage(message)) {
        this.handle(message);
      }

      received = receiveMessageOnPort(this.port);
    }
  }

  private readonly handlers: Handlers = {
    place: message => {
      this.placement = message.bounds;
    },
    view: message => this.show(message.view),
    reply: message => this.answer(message.id, message.error),
    notice: message => this.shell?.showActionNotice(message.text),
    status: message => this.shell?.showStatus(message.line),
    bringBack: () => this.shell?.bringBack(),
    close: () => this.close()
  };

  private handle(message: CoreMessage): void {
    dispatch(this.handlers, message);
  }

  private show(view: View): void {
    if (this.shell === null) {
      this.open(view);
      return;
    }

    this.shell.render(view);
  }

  private answer(id: number, error: string | null): void {
    this.replies.get(id)?.(error);
    this.replies.delete(id);
  }

  private open(view: View): void {
    // Before the window exists, so the taskbar groups it as Edge+.
    joinAppUserModel(view.appUserModelId);
    const shell = new Shell(view, {
      run: action => this.run(action),
      refresh: () => this.send({ type: "refresh" }),
      restartToUpdate: () => this.run({ kind: "restartToUpdate" }),
      reportBounds: bounds => this.send({
        type: "bounds",
        bounds
      }),
      setTimeout: (count, callback) => this.after(count, callback)
    }, this.placement);
    shell.onClosed(exitApplication);
    this.shell = shell;
    const isBackgroundRun = process.env[backgroundVariable] === "1";
    if (isBackgroundRun) {
      shell.showInBackground();
    } else {
      shell.activate();
    }
  }

  private close(): void {
    if (this.shell === null) {
      exitApplication();
      return;
    }

    this.shell.close();
  }

  private run(action: Action): Promise<string | null> {
    const id = this.nextId;
    this.nextId++;
    const { promise, resolve } = Promise.withResolvers<string | null>();
    this.replies.set(id, resolve);
    this.send({
      type: "action",
      id,
      action
    });
    return promise;
  }

  private send(message: UiMessage): void {
    this.port.postMessage(message);
  }

  private after(count: number, callback: () => void): () => void {
    const timer = this.dispatcher.createTimer();
    timer.interval = milliseconds(count);
    timer.isRepeating = false;
    timer.onTick(() => {
      this.timers.delete(timer);
      callback();
    });
    this.timers.add(timer);
    timer.start();
    return () => {
      timer.stop();
      this.timers.delete(timer);
    };
  }
}

// onStart runs on the UI thread once WinUI is up, for work of the caller's
// own that needs the dispatcher.
export async function runSession(port: MessagePort, onStart?: () => void): Promise<void> {
  await runWindow({
    onStart() {
      registerControls(winui);
      setClassHandler(applyStyleClass);
      installStyles();
      onStart?.();
      new Session(port).start();
    }
  });
}
