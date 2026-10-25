import type { Action, Bounds, CoreMessage, UiMessage } from "../protocol.ts";
import type { MessagePort } from "node:worker_threads";
import { registerWinuiDispatcherQueue } from "@microsoft/dynwinrt";
import { receiveMessageOnPort } from "node:worker_threads";
import { milliseconds } from "./layout.ts";
import { Shell } from "./shell.ts";
import { installStyles } from "./styles.ts";
import { Application, DispatcherQueue } from "./winui.ts";

// Application.start owns this thread until the window closes, so Node's event
// loop never turns here: the core's messages are taken off the port by a
// dispatcher timer, and every timer is the dispatcher's.
const inboxMilliseconds = 30;

// The core's own process, so its messages are trusted to be well formed.
function isCoreMessage(value: unknown): value is CoreMessage {
  return typeof value === "object" && value !== null && "type" in value;
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

  private handle(message: CoreMessage): void {
    switch (message.type) {
      case "place":
        this.placement = message.bounds;
        break;
      case "view":
        if (this.shell === null) {
          this.open(message.view);
        } else {
          this.shell.render(message.view);
        }
        break;
      case "reply":
        this.replies.get(message.id)?.(message.error);
        this.replies.delete(message.id);
        break;
      case "notice":
        this.shell?.showActionNotice(message.text);
        break;
      case "status":
        this.shell?.showStatus(message.line);
        break;
      case "bringBack":
        this.shell?.bringBack();
        break;
      case "close":
        this.close();
        break;
    }
  }

  private open(view: Parameters<Shell["render"]>[0]): void {
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
    shell.onClosed(() => Application.current.exit());
    this.shell = shell;
    shell.activate();
  }

  private close(): void {
    if (this.shell === null) {
      Application.current.exit();
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

export function runWindow(port: MessagePort): void {
  Application.start(() => {
    Application.create(() => {
      registerWinuiDispatcherQueue();
      installStyles();
      new Session(port).start();
    });
  });
}
