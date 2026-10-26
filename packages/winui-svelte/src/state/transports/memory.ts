import type { Frame as MessagingFrame } from "../../messaging/frame.ts";
import type { Json } from "../../messaging/json.ts";
import { jsonSchema } from "../../messaging/json.ts";
import type { Transport, TransportListener } from "./transport.ts";

// Two ends of an in-memory link, for tests and for a host and a window in one
// process. Delivery is asynchronous (a microtask) and ordered, like a real
// transport, so code written against it cannot rely on synchronous delivery.
// What is sent is copied through JSON, as any real link would.

type Delivery = (listener: TransportListener) => void;

export class MemoryEnd<Frame = MessagingFrame> implements Transport<Frame> {
  private peer: MemoryEnd<Frame> | null = null;
  private listener: TransportListener | null = null;
  private readonly inbox: Delivery[] = [];

  static pair<Frame = MessagingFrame>(): [MemoryEnd<Frame>, MemoryEnd<Frame>] {
    const left = new MemoryEnd<Frame>();
    const right = new MemoryEnd<Frame>();
    left.peer = right;
    right.peer = left;
    return [left, right];
  }

  send(frame: Frame): void {
    const copy = jsonSchema.parse(JSON.parse(JSON.stringify(frame)));
    this.peer?.receive(listener => listener.message(copy));
  }

  // A JSON value that is no frame, which the other side must report (the
  // messaging tests send it; fallow does not trace the tests).
  // fallow-ignore-next-line unused-class-member
  sendJson(value: Json): void {
    this.peer?.receive(listener => listener.message(value));
  }

  // A line the other side could not read.
  sendMalformed(reason: string): void {
    this.peer?.receive(listener => listener.malformed(reason));
  }

  // Ends this side: the other side is told the link closed.
  close(): void {
    this.peer?.receive(listener => listener.closed());
  }

  listen(listener: TransportListener): () => void {
    this.listener = listener;
    queueMicrotask(() => this.flush());
    return () => {
      this.listener = null;
    };
  }

  private receive(delivery: Delivery): void {
    this.inbox.push(delivery);
    queueMicrotask(() => this.flush());
  }

  private flush(): void {
    while (this.listener !== null && this.inbox.length > 0) {
      this.inbox.shift()?.(this.listener);
    }
  }
}
