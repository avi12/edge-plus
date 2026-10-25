// The wire protocol between a host (Rust, Electron, Node) and the window: one
// JSON value per message, in order, over any transport (transports/).
//
//   host   -> window  snapshot  the whole value of a named state
//                     patch     changes to it, applied in place
//                     reply     the end of one action
//   window -> host    action    something the user did, with an id
//
// Messages are checked structurally by the guards below; a message that fails
// them is reported and dropped, never thrown into the transport's loop.

export type Json = null | boolean | number | string | Json[] | { [key: string]: Json };

export type PathSegment = string | number;

export const MessageType = {
  snapshot: "snapshot",
  patch: "patch",
  reply: "reply",
  action: "action"
} as const;

export const PatchOperation = {
  // Sets the value at `path` (an object key, an array index, or the root
  // when the path is empty).
  set: "set",
  // Removes an object key; for an array index, removes that element.
  remove: "remove",
  // Array only: removes `deleteCount` elements at `start`, inserts `items`.
  splice: "splice"
} as const;

export type PatchOp =
  | {
    op: typeof PatchOperation.set;
    path: PathSegment[];
    value: Json;
  }
  | {
    op: typeof PatchOperation.remove;
    path: PathSegment[];
  }
  | {
    op: typeof PatchOperation.splice;
    path: PathSegment[];
    start: number;
    deleteCount: number;
    items: Json[];
  };

export interface SnapshotMessage {
  type: typeof MessageType.snapshot;
  name: string;
  value: Json;
}

export interface PatchMessage {
  type: typeof MessageType.patch;
  name: string;
  ops: PatchOp[];
}

// `error` is null when the host did what the action asked.
export interface ReplyMessage {
  type: typeof MessageType.reply;
  id: number;
  error: string | null;
}

export interface ActionMessage<Action = Json> {
  type: typeof MessageType.action;
  id: number;
  action: Action;
}

export type HostMessage = SnapshotMessage | PatchMessage | ReplyMessage;

export function isJsonObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isJson(value: unknown): value is Json {
  if (value === null) {
    return true;
  }

  const isScalar = typeof value === "boolean" || typeof value === "string" || (typeof value === "number" && Number.isFinite(value));
  if (isScalar) {
    return true;
  }

  if (Array.isArray(value)) {
    return value.every(isJson);
  }

  return isJsonObject(value) && Object.values(value).every(isJson);
}

function isPath(value: unknown): value is PathSegment[] {
  return Array.isArray(value) && value.every(segment => typeof segment === "string" || Number.isInteger(segment));
}

function isCount(value: unknown): value is number {
  return typeof value === "number" && Number.isInteger(value) && value >= 0;
}

function isPatchOp(value: unknown): value is PatchOp {
  const hasPath = isJsonObject(value) && isPath(value["path"]);
  if (!hasPath) {
    return false;
  }

  switch (value["op"]) {
    case PatchOperation.set:
      return isJson(value["value"]);
    case PatchOperation.remove:
      return true;
    case PatchOperation.splice:
      return isCount(value["start"]) && isCount(value["deleteCount"]) && Array.isArray(value["items"]) && value["items"].every(isJson);
    default:
      return false;
  }
}

export function isHostMessage(value: unknown): value is HostMessage {
  if (!isJsonObject(value)) {
    return false;
  }

  switch (value["type"]) {
    case MessageType.snapshot:
      return typeof value["name"] === "string" && isJson(value["value"]);
    case MessageType.patch:
      return typeof value["name"] === "string" && Array.isArray(value["ops"]) && value["ops"].every(isPatchOp);
    case MessageType.reply: {
      const error = value["error"];
      const hasValidError = error === null || error === undefined || typeof error === "string";
      return isCount(value["id"]) && hasValidError;
    }
    default:
      return false;
  }
}

// A host's own, unrelated messages (Edge+'s place, notice, close) are not
// malformed: the channel hands them to its `onOther` listener.
export function isTypedMessage(value: unknown): value is Record<string, unknown> & { type: string } {
  return isJsonObject(value) && typeof value["type"] === "string";
}

export function isOwnMessageType(type: string): boolean {
  return type === MessageType.snapshot || type === MessageType.patch || type === MessageType.reply;
}
