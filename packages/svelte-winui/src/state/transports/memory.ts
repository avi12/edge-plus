import type { Transport, TransportListener } from "./transport.ts";

// Two ends of an in-memory link, for tests and for a host and a window in one
// process. Delivery is asynchronous (a microtask) and ordered, like a real
// transport, so code written against it cannot rely on synchronous delivery.
// What is sent is copied through JSON, as any real link would.

type Delivery = (listener: TransportListener) => void;

export class MemoryEnd implements Transport {
  private peer: MemoryEnd | null = null;
  private listener: TransportListener | null = null;
  private readonly inbox: Delivery[] = [];

  static pair(): [MemoryEnd, MemoryEnd] {
    const left = new MemoryEnd();
    const right = new MemoryEnd();
    left.peer = right;
    right.peer = left;
    return [left, right];
  }

  send(value: unknown): void {
    const copy: unknown = JSON.parse(JSON.stringify(value));
    this.peer?.receive(listener => listener.message(copy));
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
