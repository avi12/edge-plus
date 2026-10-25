// What the channel needs of a link to the host: JSON values in order each way.
// Each transport (stdio, a worker's MessagePort, in memory) is one small file.

export interface TransportListener {
  message(value: unknown): void;
  // A line or message that could not be read (not JSON, say).
  malformed(reason: string): void;
  // The host is gone: nothing more arrives.
  closed(): void;
}

export interface Transport {
  send(value: unknown): void;
  // Starts delivering to `listener`; the returned function stops it.
  listen(listener: TransportListener): () => void;
}
