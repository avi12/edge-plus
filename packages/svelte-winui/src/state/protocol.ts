// The state protocol: a ProtocolMap (messaging/) keyed by the message types
// below. Over any transport (transports/) it is the frames messaging/ defines,
// one JSON value per message, in order:
//
//   host   -> window  snapshot  the whole value of a named state
//                     patch     changes to it, applied in place
//   window -> host    action    something the user did; its answer is the
//                     promise: resolved when the host did it, rejected with
//                     the host's error text when it could not
//
// Data is checked by the zod schemas below; a message that fails them
// is answered with an error and reported, never thrown into the transport's
// loop.

import { jsonSchema } from "../messaging/json.ts";
import type { Json } from "../messaging/json.ts";
import type { ProtocolWithReturn } from "../messaging/protocol-map.ts";
import { z } from "zod";

export type { Json };

const pathSegmentSchema = z.union([z.string(), z.int()]);

export type PathSegment = z.infer<typeof pathSegmentSchema>;

export const MessageType = {
  snapshot: "snapshot",
  patch: "patch",
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

const pathSchema = z.array(pathSegmentSchema);
const countSchema = z.int().nonnegative();

const patchOpSchema = z.discriminatedUnion("op", [
  z.object({
    op: z.literal(PatchOperation.set),
    path: pathSchema,
    value: jsonSchema
  }),
  z.object({
    op: z.literal(PatchOperation.remove),
    path: pathSchema
  }),
  z.object({
    op: z.literal(PatchOperation.splice),
    path: pathSchema,
    start: countSchema,
    deleteCount: countSchema,
    items: z.array(jsonSchema)
  })
]);

export const snapshotDataSchema = z.object({
  name: z.string(),
  value: jsonSchema
});

export const patchDataSchema = z.object({
  name: z.string(),
  ops: z.array(patchOpSchema)
});

export type PatchOp = z.infer<typeof patchOpSchema>;
export type SnapshotData = z.infer<typeof snapshotDataSchema>;
export type PatchData = z.infer<typeof patchDataSchema>;

// `Action` is the app's own action type (Edge+ generates it from Rust).
export interface StateProtocol<Action extends Json = Json> {
  [MessageType.snapshot]: ProtocolWithReturn<SnapshotData, void>;
  [MessageType.patch]: ProtocolWithReturn<PatchData, void>;
  [MessageType.action]: ProtocolWithReturn<Action, void>;
}

export function isJsonObject(value: Json | undefined): value is Record<string, Json> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}
