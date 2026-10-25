import { applyOps } from "./patch.ts";
import type { ActionMessage, HostMessage, Json } from "./protocol.ts";
import {
  isHostMessage,
  isOwnMessageType,
  isTypedMessage,
  MessageType,
  PatchOperation
} from "./protocol.ts";
import type { Transport } from "./transports/transport.ts";

// The window's end of the state the host owns. `hostState<View>(channel,
// "view")` is one reactive object: the host sends a snapshot, then patches,
// and each is applied in place, so only the bindings that read a changed
// property update. `channel.send(action)` returns a promise the host's reply
// settles. The generic parameters are the app's own types (Edge+ generates
// them from Rust), which the channel cannot check at run time beyond "this is
// JSON": the host is trusted to send its declared shape.

export class HostError extends Error {
  override readonly name = "HostError";
}

export class HostClosedError extends Error {
  override readonly name = "HostClosedError";
}

export interface HostState<State> {
  // The host's value, deeply reactive; undefined until the first snapshot.
  readonly current: State | undefined;
  readonly isReady: boolean;
  // Settles with the value once the first snapshot has arrived.
  readonly ready: Promise<State>;
}

export interface ChannelOptions {
  // Anything wrong with what the host sent or with applying it; the host's
  // remedy is a new snapshot. Defaults to a line on stderr (never stdout).
  onError?(error: Error): void;
  // A message of the host's own that is not state (Edge+'s place, notice).
  onOther?(message: Record<string, unknown>): void;
}

export interface HostChannel {
  state<State>(name: string): HostState<State>;
  send<Action>(action: Action): Promise<void>;
  dispose(): void;
}

class StateSlot<State> implements HostState<State> {
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

interface PendingAction {
  resolve(): void;
  reject(error: Error): void;
}

function writeToStderr(error: Error): void {
  process.stderr.write(`[svelte-winui] ${error.message}\n`);
}

function describe(error: unknown): Error {
  return error instanceof Error ? error : new Error(String(error));
}

class Channel implements HostChannel {
  private readonly transport: Transport;
  private readonly options: ChannelOptions;
  // Typed by whoever asks first: the one place the app's own State type
  // meets the wire (an `instanceof` hands the slot back as that State).
  private readonly slots = new Map<string, unknown>();
  private readonly pending = new Map<number, PendingAction>();
  private readonly stopListening: () => void;
  private nextId = 1;
  private isClosed = false;

  constructor(transport: Transport, options: ChannelOptions) {
    this.transport = transport;
    this.options = options;
    this.stopListening = transport.listen({
      message: value => this.receive(value),
      malformed: reason => this.report(new Error(`unreadable message: ${reason}`)),
      closed: () => this.close()
    });
  }

  state<State>(name: string): HostState<State> {
    return this.slotFor<State>(name);
  }

  private slotFor<State>(name: string): StateSlot<State> {
    const existing = this.slots.get(name);
    if (existing instanceof StateSlot) {
      return existing;
    }

    const slot = new StateSlot<State>();
    this.slots.set(name, slot);
    return slot;
  }

  send<Action>(action: Action): Promise<void> {
    if (this.isClosed) {
      return Promise.reject(new HostClosedError("the host is gone"));
    }

    const id = this.nextId;
    this.nextId++;
    const { promise, resolve, reject } = Promise.withResolvers<void>();
    this.pending.set(id, {
      resolve,
      reject
    });
    const message: ActionMessage<Action> = {
      type: MessageType.action,
      id,
      action
    };
    this.transport.send(message);
    return promise;
  }

  dispose(): void {
    this.stopListening();
    this.close();
  }

  private close(): void {
    this.isClosed = true;
    for (const action of this.pending.values()) {
      action.reject(new HostClosedError("the host closed before it replied"));
    }

    this.pending.clear();
  }

  private report(error: Error): void {
    (this.options.onError ?? writeToStderr)(error);
  }

  private receive(value: unknown): void {
    const isOtherMessage = isTypedMessage(value) && !isOwnMessageType(value["type"]);
    if (isOtherMessage) {
      this.options.onOther?.(value);
      return;
    }

    if (!isHostMessage(value)) {
      this.report(new Error("a message of the protocol's own types was malformed"));
      return;
    }

    try {
      this.handle(value);
    } catch (error) {
      this.report(describe(error));
    }
  }

  private handle(message: HostMessage): void {
    if (message.type === MessageType.reply) {
      this.answer(message.id, message.error ?? null);
      return;
    }

    if (message.type === MessageType.snapshot) {
      this.slotFor<unknown>(message.name).receiveSnapshot(message.value);
      return;
    }

    const slot = this.slots.get(message.name);
    const hasSnapshot = slot instanceof StateSlot && slot.isReady;
    if (!hasSnapshot) {
      throw new Error(`a patch for "${message.name}" arrived before its snapshot`);
    }

    applyOps(slot.cell, message.ops);
  }

  private answer(id: number, error: string | null): void {
    const action = this.pending.get(id);
    if (action === undefined) {
      throw new Error(`a reply for action ${id}, which is not waiting`);
    }

    this.pending.delete(id);

    if (error === null) {
      action.resolve();
    } else {
      action.reject(new HostError(error));
    }
  }
}

export function createHostChannel(transport: Transport, options: ChannelOptions = {}): HostChannel {
  return new Channel(transport, options);
}

export function hostState<State>(channel: HostChannel, name: string): HostState<State> {
  return channel.state<State>(name);
}
