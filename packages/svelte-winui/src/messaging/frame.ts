import { jsonSchema } from "./json.ts";
import { z } from "zod";

// What travels over a transport, as @webext-core/messaging's frames do:
//   request   { id, type, data, timestamp }
//   response  { id, response } or { id, error }
// A frame with a `type` is a request; one without is the answer to the
// request with the same id.

const identifierSchema = z.int().nonnegative();

export const requestFrameSchema = z.object({
  id: identifierSchema,
  type: z.string(),
  data: jsonSchema.optional(),
  timestamp: z.number().optional()
});

export const responseFrameSchema = z.object({
  id: identifierSchema,
  // A response has no type: that is what tells it from a request.
  type: z.undefined().optional(),
  response: jsonSchema.optional(),
  // Null or absent when the handler returned.
  error: z.string().nullish()
});

export type RequestFrame = z.infer<typeof requestFrameSchema>;
export type ResponseFrame = z.infer<typeof responseFrameSchema>;
// Both kinds: what a link carries for messaging.
export type Frame = RequestFrame | ResponseFrame;
