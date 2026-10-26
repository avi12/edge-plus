// A compile error in a .svelte file as a line a developer can click:
// `file:line:column: message` and the code around it. Svelte's own errors
// carry the pieces; Vite wraps them, keeping them on the error it throws.

import { z } from "zod";

export interface CompileDiagnostic {
  file: string;
  line: number;
  column: number;
  code: string;
  text: string;
  frame: string;
}

const positionSchema = z.object({
  line: z.number(),
  column: z.number(),
  file: z.string().optional().catch(undefined)
});

type Position = z.infer<typeof positionSchema>;

// A field of the wrong type reads as absent, as the error may be anything a
// compiler or plugin threw.
function lenient<TSchema extends z.ZodType>(schema: TSchema) {
  return schema.optional().catch(undefined);
}

// The fields of Svelte's error that a diagnostic is made of; Vite's copies
// them onto its own error, or keeps Svelte's on `.cause`.
const sourceSchema = z.object({
  start: lenient(positionSchema),
  loc: lenient(positionSchema),
  filename: lenient(z.string()),
  id: lenient(z.string()),
  code: lenient(z.string()),
  message: lenient(z.string()),
  frame: lenient(z.string()),
  cause: lenient(z.instanceof(Error))
});

type Source = z.infer<typeof sourceSchema>;

function sourceOf(error: Error): Source | null {
  const source = sourceSchema.parse(error);
  const hasPosition = source.start !== undefined || source.loc !== undefined;
  if (hasPosition) {
    return source;
  }

  return source.cause === undefined ? null : sourceOf(source.cause);
}

function positionOf(source: Source): Position | null {
  return source.start ?? source.loc ?? null;
}

// Vite's message starts with the position again.
const leadingPosition = /^\S+:\d+:\d+\s+/u;

// The message without the documentation link Svelte appends on its own line.
function firstLine(message: string): string {
  return (message.split("\n")[0] ?? message).replace(leadingPosition, "");
}

export function compileDiagnostic(error: Error): CompileDiagnostic | null {
  const source = sourceOf(error);
  const position = source === null ? null : positionOf(source);
  const isLocated = source !== null && position !== null;
  if (!isLocated) {
    return null;
  }

  const file = source.filename ?? source.id ?? source.loc?.file ?? "";
  return {
    file,
    line: position.line,
    // Svelte counts columns from 0; editors from 1.
    column: position.column + 1,
    code: source.code ?? "",
    text: firstLine(source.message ?? "compile error"),
    frame: source.frame ?? ""
  };
}

export function formatDiagnostic(diagnostic: CompileDiagnostic): string {
  const code = diagnostic.code === "" ? "" : ` [${diagnostic.code}]`;
  const header = `${diagnostic.file}:${diagnostic.line}:${diagnostic.column}: ${diagnostic.text}${code}`;
  return diagnostic.frame === "" ? header : `${header}\n${diagnostic.frame}`;
}

// What an error reads as on stderr: a compile error with its position, any
// other error as it is.
// A value caught or rejected with, as the Error it is or one made of its text.
export function errorOf<TThrown>(thrown: TThrown): Error {
  return thrown instanceof Error ? thrown : new Error(String(thrown));
}

export function describeError(error: Error | string): string {
  if (typeof error === "string") {
    return error;
  }

  const diagnostic = compileDiagnostic(error);
  if (diagnostic !== null) {
    return formatDiagnostic(diagnostic);
  }

  return error.stack ?? error.message;
}
