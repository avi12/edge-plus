// What a host written in JavaScript (a Node process, Electron's main process)
// needs to own state a window shows: the protocol (`StateProtocol`, `MessageType`), `diff` for the patches, and
// the transports. Nothing here uses runes, so it loads in plain Node; the
// window's side (`hostState`, `createHostChannel`) is `svelte-winui/state`.

export { diff } from "./diff.ts";
export type {
  Json,
  PatchData,
  PatchOp,
  PathSegment,
  SnapshotData,
  StateProtocol
} from "./protocol.ts";
export { isJsonObject, MessageType, PatchOperation } from "./protocol.ts";
export type { MessagePortOptions } from "./transports/message-port.ts";
export { createMessagePortTransport } from "./transports/message-port.ts";
export type { StdioStreams } from "./transports/stdio.ts";
export { createStdioTransport, reserveStdout } from "./transports/stdio.ts";
export type { Transport, TransportListener } from "./transports/transport.ts";
