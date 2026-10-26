import type { Transport } from "../state/transports/transport.ts";
import type { Frame, RequestFrame, ResponseFrame } from "./frame.ts";
import { requestFrameSchema, responseFrameSchema } from "./frame.ts";
import type { WireValue } from "./json.ts";
import type { GetReturnType, MessageHandler, RemoveListener, SendArguments } from "./protocol-map.ts";

// defineMessaging(transport) is webext-core's defineExtensionMessaging for
// any link the state channel's transports provide (stdio, a worker's
// MessagePort, memory): sendMessage(type, data) resolves with what the other
// end's onMessage(type, handler) returns, and rejects with what it threw.

export class MessagingError extends Error {
  override readonly name: string = "MessagingError";
}

export class MessagingClosedError extends MessagingError {
  override readonly name: string = "MessagingClosedError";

  constructor() {
    super("the link closed before the message was answered");
  }
}

export interface MessagingOptions {
  // A request unanswered for this long rejects; unlimited when absent.
  timeoutMilliseconds?: number;
  // A request whose type has no handler here is answered with an error; this
  // also reports it.
  onUnhandled?(frame: RequestFrame): void;
  // A frame that is neither a request nor a response, or a handler that threw.
  onError?(error: Error): void;
  // The current time for a request's timestamp.
  now?(): number;
}

export interface Messaging<TProtocolMap> {
  sendMessage<TType extends keyof TProtocolMap & string>(
    ...parameters: SendArguments<TProtocolMap, TType>
  ): Promise<GetReturnType<TProtocolMap[TType]>>;
  onMessage<TType extends keyof TProtocolMap & string>(
    type: TType,
    handler: MessageHandler<TProtocolMap, TType>
  ): RemoveListener;
  removeAllListeners(): void;
  // Stops listening and rejects what is still waiting.
  close(): void;
}

interface Pending {
  resolve(value: WireValue): void;
  reject(error: Error): void;
  timer: ReturnType<typeof setTimeout> | null;
}

export function defineMessaging<TProtocolMap>(
  transport: Transport<Frame>,
  options: MessagingOptions = {}
): Messaging<TProtocolMap> {
  const { now = Date.now, onError = () => {}, onUnhandled = () => {}, timeoutMilliseconds } = options;
  const pending = new Map<number, Pending>();
  const handlers = new Map<string, (frame: RequestFrame) => WireValue | Promise<WireValue>>();
  let nextIdentifier = 0;

  function respond(frame: ResponseFrame): void {
    transport.send(frame);
  }

  async function answer(request: RequestFrame): Promise<void> {
    const handler = handlers.get(request.type);
    if (handler === undefined) {
      onUnhandled(request);
      respond({
        id: request.id,
        error: `no handler for message "${request.type}"`
      });
      return;
    }

    try {
      const response = await handler(request);
      respond({
        id: request.id,
        response: response === undefined ? null : response
      });
    } catch (error) {
      const failure = error instanceof Error ? error : new MessagingError(String(error));
      onError(failure);
      respond({
        id: request.id,
        error: failure.message
      });
    }
  }

  function clearTimer(waiting: Pending): void {
    if (waiting.timer !== null) {
      clearTimeout(waiting.timer);
    }
  }

  function settle(frame: ResponseFrame): void {
    const waiting = pending.get(frame.id);
    if (waiting === undefined) {
      return;
    }

    pending.delete(frame.id);
    clearTimer(waiting);

    if (typeof frame.error === "string") {
      waiting.reject(new MessagingError(frame.error));
    } else {
      waiting.resolve(frame.response);
    }
  }

  function rejectAll(error: Error): void {
    for (const waiting of pending.values()) {
      clearTimer(waiting);
      waiting.reject(error);
    }
    pending.clear();
  }

  const stop = transport.listen({
    message(value) {
      const request = requestFrameSchema.safeParse(value);
      const response = responseFrameSchema.safeParse(value);
      if (request.success) {
        void answer(request.data);
      } else if (response.success) {
        settle(response.data);
      } else {
        onError(new MessagingError(`not a message frame: ${JSON.stringify(value)}`));
      }
    },
    malformed(reason) {
      onError(new MessagingError(reason));
    },
    closed() {
      rejectAll(new MessagingClosedError());
    }
  });

  return {
    sendMessage(...parameters) {
      const [type, data] = parameters;
      nextIdentifier += 1;
      const id = nextIdentifier;
      return new Promise((resolve, reject) => {
        const timer = timeoutMilliseconds === undefined
          ? null
          : setTimeout(() => {
            pending.delete(id);
            reject(new MessagingError(`no answer to message "${type}" in ${timeoutMilliseconds} ms`));
          }, timeoutMilliseconds);
        pending.set(id, {
          // The same trust as onMessage below: the answer is taken to be its
          // message's declared return type.
          // oxlint-disable-next-line typescript/consistent-type-assertions
          resolve: answer => resolve(answer as Parameters<typeof resolve>[0]),
          reject,
          timer
        });
        transport.send({
          id,
          type,
          data: data === undefined ? null : data,
          timestamp: now()
        });
      });
    },
    onMessage(type, handler) {
      if (handlers.has(type)) {
        throw new MessagingError(`message "${type}" already has a handler`);
      }

      // The wire carries JSON the protocol map describes but cannot check: the
      // frame's data is taken to be its message's data, as webext-core does.
      // oxlint-disable-next-line typescript/consistent-type-assertions
      handlers.set(type, frame => handler(frame as Parameters<typeof handler>[0]));
      return () => {
        handlers.delete(type);
      };
    },
    removeAllListeners() {
      handlers.clear();
    },
    close() {
      stop();
      handlers.clear();
      rejectAll(new MessagingClosedError());
    }
  };
}
