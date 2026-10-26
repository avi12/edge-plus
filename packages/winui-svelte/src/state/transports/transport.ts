// What the channel needs of a link to the host: JSON values in order each way.
// Each transport (stdio, a worker's MessagePort, in memory) is one small file.

import type { Frame } from "../../messaging/frame.ts";
import type { Json } from "../../messaging/json.ts";

export interface TransportListener {
  message(value: Json): void;
  // A line or message that could not be read (not JSON, say).
  malformed(reason: string): void;
  // The host is gone: nothing more arrives.
  closed(): void;
}

// `Sent` is what this end sends (messaging's frames by default); what arrives is JSON.
export interface Transport<Sent = Frame> {
  send(frame: Sent): void;
  // Starts delivering to `listener`; the returned function stops it.
  listen(listener: TransportListener): () => void;
}
