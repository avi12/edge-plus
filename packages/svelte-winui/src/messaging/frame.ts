import { isJsonObject } from "../state/protocol.ts";

// What travels over a transport, as @webext-core/messaging's frames do:
//   request   { id, type, data, timestamp }
//   response  { id, response } or { id, error }
// A frame with a `type` is a request; one without is the answer to the
// request with the same id.

export interface RequestFrame {
  id: number;
  type: string;
  data: unknown;
  timestamp: number;
}

export interface ResponseFrame {
  id: number;
  response?: unknown;
  // Null or absent when the handler returned.
  error?: string | null;
}

function isIdentifier(value: unknown): value is number {
  return typeof value === "number" && Number.isInteger(value) && value >= 0;
}

export function isRequestFrame(value: unknown): value is RequestFrame {
  return isJsonObject(value) && isIdentifier(value["id"]) && typeof value["type"] === "string";
}

export function isResponseFrame(value: unknown): value is ResponseFrame {
  if (!isJsonObject(value)) {
    return false;
  }

  const error = value["error"];
  const hasValidError = error === undefined || error === null || typeof error === "string";
  return isIdentifier(value["id"]) && !("type" in value) && hasValidError;
}
