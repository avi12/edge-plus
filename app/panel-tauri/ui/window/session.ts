import type { Action, Bounds, UiMessage, View } from "../protocol.g.ts";
import type { OtherMessage } from "./core-link.ts";
import { isOtherMessage, viewAsSnapshot, viewStateName } from "./core-link.ts";
import { milliseconds } from "./layout.ts";
import { joinAppUserModel } from "./native.ts";
import "./shared-styles.svelte";
import { Shell } from "./shell.ts";
import { installStyles } from "./styles.ts";
import * as winui from "./winui.ts";
import { DispatcherQueue } from "./winui.ts";
import type { MessagePort } from "node:worker_threads";
import { registerControls } from "svelte-winui";
import type { HostChannel, HostState, Transport } from "svelte-winui/state";
import { createHostChannel, createMessagePortTransport, hostState } from "svelte-winui/state";
import { exitApplication, runWindow } from "svelte-winui/window";

// WinUI's loop owns this thread until the window closes, so Node's event loop
// never turns here: the core's messages are taken off the port by a
// dispatcher timer (the channel's `poll`), and every timer is the dispatcher's.
const inboxMilliseconds = 30;
// Set by the dev fixture for unattended captures: the window opens without
// taking focus (the fixture also places it off screen).
const backgroundVariable = "EDGEPLUS_WINDOW_BACKGROUND";

type MessageType = OtherMessage["type"];
type MessageOfType = { [Message in OtherMessage as Message["type"]]: Message };
type Handlers = { [Type in MessageType]: (message: MessageOfType[Type]) => void };

function dispatch<Type extends MessageType>(handlers: Handlers, message: MessageOfType[Type] & { type: Type }): void {
  handlers[message.type](message);
}

class Session {
  private readonly dispatcher = DispatcherQueue.getForCurrentThread();
  // Held here, as is every pending timer: a timer whose wrapper is collected
  // stops.
  private readonly inbox = this.dispatcher.createTimer();
  private readonly timers = new Set<ReturnType<DispatcherQueue["createTimer"]>>();
  private readonly transport: Transport;
  private readonly channel: HostChannel;
  // The core's View, one reactive object the page reads; the core sends it
  // whole each time and the channel changes only what differs.
  private readonly view: HostState<View>;
  private shell: Shell | null = null;
  private placement: Bounds | null = null;

  constructor(port: MessagePort) {
    this.transport = viewAsSnapshot(createMessagePortTransport(port, { poll: drain => this.poll(drain) }));
    this.channel = createHostChannel(this.transport, { onOther: message => this.handle(message) });
    this.view = hostState<View>(this.channel, viewStateName);
  }

  start(): void {
    void this.view.ready.then(view => {
      this.open(view);
    });
  }

  private poll(drain: () => void): () => void {
    this.inbox.interval = milliseconds(inboxMilliseconds);
    this.inbox.isRepeating = true;
    this.inbox.onTick(drain);
    this.inbox.start();
    return () => {
      this.inbox.stop();
    };
  }

  private readonly handlers: Handlers = {
    place: message => {
      this.placement = message.bounds;
    },
    notice: message => this.shell?.showActionNotice(message.text),
    status: message => this.shell?.showStatus(message.line),
    bringBack: () => this.shell?.bringBack(),
    close: () => this.close()
  };

  private handle(message: unknown): void {
    if (isOtherMessage(message)) {
      dispatch(this.handlers, message);
    }
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

  // Resolves to why the core refused the action, or null.
  private async run(action: Action): Promise<string | null> {
    try {
      await this.channel.send(action);
      return null;
    } catch (error) {
      return error instanceof Error ? error.message : String(error);
    }
  }

  private send(message: UiMessage): void {
    this.transport.send(message);
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
      installStyles();
      onStart?.();
      new Session(port).start();
    }
  });
}
