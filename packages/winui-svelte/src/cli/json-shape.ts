import { jsonSchema } from "../messaging/json.ts";
import type { Json } from "../messaging/json.ts";
import { z } from "zod";

// The two shapes a parsed package.json or catalogue is walked through: each
// returns undefined where the value is not that shape, so a caller falls back
// or throws as it needs.

const recordSchema = z.record(z.string(), jsonSchema);
const listSchema = z.array(jsonSchema);

export function readRecord(value: Json | undefined): Record<string, Json> | undefined {
  const record = recordSchema.safeParse(value);
  return record.success ? record.data : undefined;
}

export function readList(value: Json | undefined): Json[] | undefined {
  const list = listSchema.safeParse(value);
  return list.success ? list.data : undefined;
}
