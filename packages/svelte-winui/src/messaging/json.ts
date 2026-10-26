import { z } from "zod";

// What can cross a link: a JSON value. Everything read off a wire is checked
// by this schema (or a narrower one) before the rest of the code sees it.

export const jsonSchema = z.json();

export type Json = z.infer<typeof jsonSchema>;

// What a message carries or answers with: a JSON value, or nothing (`void`).
export type WireValue = Json | void;
