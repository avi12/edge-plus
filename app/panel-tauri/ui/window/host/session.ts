import type { Action, Bounds, EdgePlusProtocolMap, View } from "../../protocol.g.ts";
import { CoreMessageType, UiMessageType } from "../../protocol.g.ts";
import Shell from "../Shell.svelte";
import { milliseconds } from "../style/layout.ts";
import "../style/SharedStyles.svelte";
import { installStyles } from "../style/styles.ts";
import * as winui from "../winui.ts";
import { DispatcherQueue } from "../winui.ts";
import { viewAsSnapshot, viewStateName } from "./core-link.ts";
import { Panel } from "./panel.svelte.ts";
import type { MessagePort } from "node:worker_threads";
import { mountControl, registerControls } from "svelte-winui";
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

// The messages the channel does not take for itself: its `view` is the
// channel's snapshot (core-link.ts) and its `action` is channel.send.
type SessionProtocol = Omit<EdgePlusProtocolMap, typeof CoreMessageType.view | typeof UiMessageType.action>;

class Session {
  private readonly dispatcher = DispatcherQueue.getForCurrentThread();
  // Held here, as is every pending timer: a timer whose wrapper is collected
  // stops.
  private readonly inbox = this.dispatcher.createTimer();
  private readonly timers = new Set<ReturnType<DispatcherQueue["createTimer"]>>();
  private readonly transport: Transport;
  private readonly channel: HostChannel<Action, SessionProtocol>;
  // The core's View, one reactive object the page reads; the core sends it
  // whole each time and the channel changes only what differs.
  private readonly view: HostState<View>;
  private readonly panel = new Panel({
    run: action => this.run(action),
    restartToUpdate: () => this.run({ kind: "restartToUpdate" }),
    after: (count, callback) => this.after(count, callback)
  });
  private placement: Bounds | null = null;

  constructor(port: MessagePort) {
    this.transport = viewAsSnapshot(createMessagePortTransport(port, { poll: drain => this.poll(drain) }));
    this.channel = createHostChannel<Action, SessionProtocol>(this.transport);
    this.view = hostState<View>(this.channel, viewStateName);
    this.listen();
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

  private listen(): void {
    const { messaging } = this.channel;
    messaging.onMessage(CoreMessageType.place, message => {
      this.placement = message.data;
    });
    messaging.onMessage(CoreMessageType.notice, message => {
      this.panel.actionNotice = message.data;
    });
    messaging.onMessage(CoreMessageType.status, message => {
      this.panel.showStatus(message.data);
    });
    // Launching Edge+ again lands here.
    messaging.onMessage(CoreMessageType.bringBack, () => {
      this.panel.window?.bringToFront();
    });
    messaging.onMessage(CoreMessageType.close, () => {
      this.close();
    });
  }

  private open(view: View): void {
    mountControl(Shell, {
      view,
      panel: this.panel,
      placement: this.placement,
      isBackground: process.env[backgroundVariable] === "1",
      refresh: () => this.tell(this.channel.messaging.sendMessage(UiMessageType.refresh)),
      reportBounds: (bounds: Bounds) => this.tell(this.channel.messaging.sendMessage(UiMessageType.bounds, bounds)),
      onClosed: exitApplication
    });
  }

  private close(): void {
    if (this.panel.window === undefined) {
      exitApplication();
      return;
    }

    this.panel.window.close();
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

  // The window does not wait for the core to take a notice (the core's answer
  // is empty); a failure to deliver is the channel's to report.
  private tell(delivery: Promise<void>): void {
    delivery.catch(() => {});
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
