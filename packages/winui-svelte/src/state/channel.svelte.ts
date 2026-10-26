import { defineMessaging, MessagingClosedError } from "../messaging/index.ts";
import type { Messaging } from "../messaging/index.ts";
import { applyOps } from "./patch.ts";
import type { Json, StateProtocol } from "./protocol.ts";
import { MessageType, patchDataSchema, PatchOperation, snapshotDataSchema } from "./protocol.ts";
import type { Transport } from "./transports/transport.ts";

// The window's end of the state the host owns. `hostState<View>(channel,
// "view")` is one reactive object: the host sends a snapshot, then patches,
// and each is applied in place, so only the bindings that read a changed
// property update. `channel.send(action)` returns a promise the host's answer
// settles. The channel is a typed messaging link (messaging/) with the state
// protocol's handlers registered; `channel.messaging` is that link, for an app
// to register handlers for, and send, message types of its own. The generic
// parameters are the app's own types (Edge+ generates them from Rust), which
// the channel cannot check at run time beyond "this is JSON": the host is
// trusted to send its declared shape.

export class HostError extends Error {
  override readonly name = "HostError";
}

export class HostClosedError extends Error {
  override readonly name = "HostClosedError";
}

export interface HostState<State extends Json> {
  // The host's value, deeply reactive; undefined until the first snapshot.
  readonly current: State | undefined;
  readonly isReady: boolean;
  // Settles with the value once the first snapshot has arrived.
  readonly ready: Promise<State>;
}

export interface ChannelOptions {
  // Anything wrong with what the host sent or with applying it, or a message
  // type nobody handles; the host's remedy is a new snapshot. Defaults to a
  // line on stderr (never stdout).
  onError?(error: Error): void;
}

// `Action` is the type of what `send` takes; `Extra` is a ProtocolMap of the
// app's own message types, added to the state protocol's.
export interface HostChannel<Action extends Json = Json, Extra extends object = object> {
  state<State extends Json>(name: string): HostState<State>;
  // Resolves when the host did the action; rejects with HostError carrying its
  // text when it could not, and with HostClosedError when the link is gone.
  send(action: Action): Promise<void>;
  readonly messaging: Messaging<StateProtocol<Action> & Extra>;
  dispose(): void;
}

class StateSlot<State extends Json> implements HostState<State> {
  // The cell is the deep proxy; the root sits in it so a snapshot of a
  // different shape can still replace it.
  readonly cell: { current: State | undefined } = $state({ current: undefined });
  private hasSnapshot = $state(false);
  private readonly settled = Promise.withResolvers<State>();

  get current(): State | undefined {
    return this.cell.current;
  }

  get isReady(): boolean {
    return this.hasSnapshot;
  }

  get ready(): Promise<State> {
    return this.settled.promise;
  }

  receiveSnapshot(value: Json): void {
    // Written through patch.ts's untyped holder: the host's value is trusted
    // to be State.
    applyOps(this.cell, [{
      op: PatchOperation.set,
      path: [],
      value
    }]);
    this.hasSnapshot = true;

    if (this.cell.current !== undefined) {
      this.settled.resolve(this.cell.current);
    }
  }
}

function writeToStderr(error: Error): void {
  process.stderr.write(
    `[winui-svelte] ${error.message}
`
  );
}

class Channel<Action extends Json, Extra extends object> implements HostChannel<Action, Extra> {
  readonly messaging: Messaging<StateProtocol<Action> & Extra>;
  // The same link seen as the state protocol alone, which TypeScript can
  // resolve while Action and Extra are still open.
  private readonly own: Messaging<StateProtocol>;
  // Typed by whoever asks first: the one place the app's own State type
  // meets the wire (an `instanceof` hands the slot back as that State).
  private readonly slots = new Map<string, object>();
  private isClosed = false;

  constructor(transport: Transport, options: ChannelOptions) {
    const report = options.onError ?? writeToStderr;
    this.messaging = defineMessaging<StateProtocol<Action> & Extra>(transport, {
      onError: report,
      onUnhandled: frame => report(new Error(`no handler for message "${frame.type}"`))
    });
    this.own = this.messaging;
    this.own.onMessage(MessageType.snapshot, message => this.receiveSnapshot(message.data));
    this.own.onMessage(MessageType.patch, message => this.receivePatch(message.data));
  }

  state<State extends Json>(name: string): HostState<State> {
    return this.slotFor<State>(name);
  }

  private slotFor<State extends Json>(name: string): StateSlot<State> {
    const existing = this.slots.get(name);
    if (existing instanceof StateSlot) {
      return existing;
    }

    const slot = new StateSlot<State>();
    this.slots.set(name, slot);
    return slot;
  }

  async send(action: Action): Promise<void> {
    if (this.isClosed) {
      throw new HostClosedError("the host is gone");
    }

    try {
      await this.own.sendMessage(MessageType.action, action);
    } catch (error) {
      const isLinkClosed = error instanceof MessagingClosedError;
      if (isLinkClosed) {
        this.isClosed = true;
        throw new HostClosedError("the host closed before it replied");
      }

      throw new HostError(error instanceof Error ? error.message : String(error));
    }
  }

  dispose(): void {
    this.isClosed = true;
    this.messaging.close();
  }

  private receiveSnapshot(data: Json): void {
    const snapshot = snapshotDataSchema.safeParse(data);
    if (!snapshot.success) {
      throw new Error("a snapshot was malformed");
    }

    this.slotFor<Json>(snapshot.data.name).receiveSnapshot(snapshot.data.value);
  }

  private receivePatch(data: Json): void {
    const patch = patchDataSchema.safeParse(data);
    if (!patch.success) {
      throw new Error("a patch was malformed");
    }

    const slot = this.slots.get(patch.data.name);
    const hasSnapshot = slot instanceof StateSlot && slot.isReady;
    if (!hasSnapshot) {
      throw new Error(`a patch for "${patch.data.name}" arrived before its snapshot`);
    }

    applyOps(slot.cell, patch.data.ops);
  }
}

export function createHostChannel<Action extends Json = Json, Extra extends object = object>(
  transport: Transport,
  options: ChannelOptions = {}
): HostChannel<Action, Extra> {
  return new Channel<Action, Extra>(transport, options);
}

export function hostState<State extends Json>(channel: HostChannel<never, object>, name: string): HostState<State> {
  return channel.state<State>(name);
}
