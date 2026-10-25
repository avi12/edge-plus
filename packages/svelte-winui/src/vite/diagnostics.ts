// A compile error in a .svelte file as a line a developer can click:
// `file:line:column: message` and the code around it. Svelte's own errors
// carry the pieces; Vite wraps them, keeping them on the error it throws.

export interface CompileDiagnostic {
  file: string;
  line: number;
  column: number;
  code: string;
  text: string;
  frame: string;
}

interface Position {
  line: number;
  column: number;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

function isPosition(value: unknown): value is Position {
  return isRecord(value) && typeof value["line"] === "number" && typeof value["column"] === "number";
}

// Vite's error keeps Svelte's on `.cause`, or copies its fields onto itself.
function sourceOf(error: unknown): Record<string, unknown> | null {
  if (!isRecord(error)) {
    return null;
  }

  const hasPosition = isPosition(error["start"]) || isPosition(error["loc"]);
  return hasPosition ? error : sourceOf(error["cause"]);
}

function positionOf(source: Record<string, unknown>): Position | null {
  const { start, loc } = source;
  if (isPosition(start)) {
    return start;
  }

  return isPosition(loc) ? loc : null;
}

function text(value: unknown, fallback: string): string {
  return typeof value === "string" ? value : fallback;
}

// Vite's message starts with the position again.
const leadingPosition = /^\S+:\d+:\d+\s+/u;

// The message without the documentation link Svelte appends on its own line.
function firstLine(message: string): string {
  return (message.split("\n")[0] ?? message).replace(leadingPosition, "");
}

export function compileDiagnostic(error: unknown): CompileDiagnostic | null {
  const source = sourceOf(error);
  const position = source === null ? null : positionOf(source);
  const isLocated = source !== null && position !== null;
  if (!isLocated) {
    return null;
  }

  const file = text(source["filename"], text(source["id"], text(read(source["loc"], "file"), "")));
  return {
    file,
    line: position.line,
    // Svelte counts columns from 0; editors from 1.
    column: position.column + 1,
    code: text(source["code"], ""),
    text: firstLine(text(source["message"], "compile error")),
    frame: text(source["frame"], "")
  };
}

function read(value: unknown, key: string): unknown {
  return isRecord(value) ? value[key] : undefined;
}

export function formatDiagnostic(diagnostic: CompileDiagnostic): string {
  const code = diagnostic.code === "" ? "" : ` [${diagnostic.code}]`;
  const header = `${diagnostic.file}:${diagnostic.line}:${diagnostic.column}: ${diagnostic.text}${code}`;
  return diagnostic.frame === "" ? header : `${header}\n${diagnostic.frame}`;
}

// What an error reads as on stderr: a compile error with its position, any
// other error as it is.
export function describeError(error: unknown): string {
  const diagnostic = compileDiagnostic(error);
  if (diagnostic !== null) {
    return formatDiagnostic(diagnostic);
  }

  return error instanceof Error ? error.stack ?? error.message : String(error);
}
