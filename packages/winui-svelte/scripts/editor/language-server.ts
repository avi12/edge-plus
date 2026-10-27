// A client for the Svelte language server, the one VS Code's Svelte extension
// runs: it opens files the way the editor does and asks for what the editor
// shows (diagnostics and inlay hints). Set EDITOR_CHECK_DEBUG to see the
// server's own log.
import type { Json } from "../../src/messaging/json.ts";
import { jsonSchema } from "../../src/messaging/json.ts";
import { type ChildProcessWithoutNullStreams, spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import { createRequire } from "node:module";
import { pathToFileURL } from "node:url";
import { z } from "zod";

const positionSchema = z.object({
  line: z.number(),
  character: z.number()
});

const diagnosticSchema = z.object({
  range: z.object({
    start: positionSchema
  }),
  message: z.string(),
  code: z.union([z.number(), z.string()]).optional(),
  source: z.string().optional()
});

export type Diagnostic = z.infer<typeof diagnosticSchema>;

const inlayHintSchema = z.object({
  position: positionSchema,
  label: z.union([z.string(), z.array(z.object({ value: z.string() }))])
});

export type InlayHint = z.infer<typeof inlayHintSchema>;

const messageSchema = z.object({
  id: z.number().optional(),
  method: z.string().optional(),
  params: jsonSchema.optional(),
  result: jsonSchema.optional()
});

type Message = z.infer<typeof messageSchema>;

const publishedSchema = z.object({
  uri: z.string(),
  diagnostics: z.array(diagnosticSchema)
});

const configurationRequestSchema = z.object({
  items: z.array(z.object({ section: z.string().optional() }))
});

const headerSeparator = "\r\n\r\n";
const contentLengthPattern = /Content-Length: (\d+)/i;
// The server publishes a file's diagnostics more than once (Svelte's first,
// TypeScript's after); it has settled when nothing new arrives for this long.
const settleMilliseconds = 4000;
const pollMilliseconds = 250;
const settleTimeoutMilliseconds = 300_000;

// The editor's inlay hints, all on, so a type that collapsed to any shows.
const inlayHints = {
  parameterNames: {
    enabled: "all"
  },
  parameterTypes: {
    enabled: true
  },
  variableTypes: {
    enabled: true,
    suppressWhenTypeMatchesName: false
  },
  propertyDeclarationTypes: {
    enabled: true
  },
  functionLikeReturnTypes: {
    enabled: true
  }
};
const configuration = new Map<string, Json>([
  ["typescript", { inlayHints }],
  ["javascript", { inlayHints }],
  ["svelte", {}]
]);

// One key per file however the server spells its URI (it lower-cases the
// drive and escapes its colon).
function fileKey(file: string): string {
  return uriKey(pathToFileURL(file).href);
}

function uriKey(uri: string): string {
  return decodeURIComponent(uri).toLowerCase();
}

function sleep(milliseconds: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, milliseconds));
}

export class LanguageServer {
  private readonly server: ChildProcessWithoutNullStreams;
  private readonly pending = new Map<number, (result: Json | undefined) => void>();
  private readonly diagnostics = new Map<string, Diagnostic[]>();
  private nextId = 1;
  private buffer = Buffer.alloc(0);
  private lastPublish = Date.now();
  private exitCode: number | null | undefined;

  constructor(fromFolder: string) {
    const serverPath = createRequire(pathToFileURL(`${fromFolder}/`)).resolve("svelte-language-server/bin/server.js");
    this.server = spawn(process.execPath, [serverPath, "--stdio"], { stdio: ["pipe", "pipe", "pipe"] });
    this.server.stdout.on("data", (chunk: Buffer) => this.receive(chunk));
    this.server.stderr.on("data", (chunk: Buffer) => {
      if (process.env.EDITOR_CHECK_DEBUG) {
        process.stderr.write(chunk);
      }
    });
    this.server.on("exit", code => {
      this.exitCode = code;
    });
  }

  async start(rootFolder: string): Promise<void> {
    const rootUri = pathToFileURL(rootFolder).href;
    await this.request("initialize", {
      processId: process.pid,
      rootUri,
      workspaceFolders: [{
        uri: rootUri,
        name: "root"
      }],
      capabilities: {
        textDocument: {
          publishDiagnostics: {},
          inlayHint: {}
        },
        workspace: {
          configuration: true
        }
      },
      initializationOptions: {
        configuration: Object.fromEntries(configuration)
      }
    });
    this.notify("initialized", {});
  }

  open(file: string): void {
    this.notify("textDocument/didOpen", {
      textDocument: {
        uri: pathToFileURL(file).href,
        languageId: "svelte",
        version: 1,
        text: readFileSync(file, "utf8")
      }
    });
  }

  // Waits until every opened file has diagnostics and none arrived lately.
  async settle(files: string[]): Promise<void> {
    const deadline = Date.now() + settleTimeoutMilliseconds;
    while (Date.now() < deadline) {
      if (this.exitCode !== undefined) {
        throw new Error(`The Svelte language server exited (${String(this.exitCode)}).`);
      }

      const hasEveryFile = files.every(file => this.diagnostics.has(fileKey(file)));
      const isSettled = hasEveryFile && Date.now() - this.lastPublish > settleMilliseconds;
      if (isSettled) {
        return;
      }

      await sleep(pollMilliseconds);
    }

    throw new Error("The Svelte language server did not finish checking in time.");
  }

  diagnosticsOf(file: string): Diagnostic[] {
    return this.diagnostics.get(fileKey(file)) ?? [];
  }

  async inlayHintsOf(file: string): Promise<InlayHint[]> {
    const lineCount = readFileSync(file, "utf8").split("\n").length;
    const hints = await this.request("textDocument/inlayHint", {
      textDocument: {
        uri: pathToFileURL(file).href
      },
      range: {
        start: {
          line: 0,
          character: 0
        },
        end: {
          line: lineCount,
          character: 0
        }
      }
    });
    return z.array(inlayHintSchema).nullable().parse(hints) ?? [];
  }

  stop(): void {
    this.server.kill();
  }

  private request(method: string, params: Json): Promise<Json | undefined> {
    const id = this.nextId;
    this.nextId++;
    this.send({
      id,
      method,
      params
    });
    return new Promise(resolve => this.pending.set(id, resolve));
  }

  private notify(method: string, params: Json): void {
    this.send({
      method,
      params
    });
  }

  private send(message: Message): void {
    const body = Buffer.from(
      JSON.stringify({
        jsonrpc: "2.0",
        ...message
      }), "utf8"
    );
    this.server.stdin.write(`Content-Length: ${body.length}${headerSeparator}`);
    this.server.stdin.write(body);
  }

  private receive(chunk: Buffer): void {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const headerEnd = this.buffer.indexOf(headerSeparator);
      if (headerEnd < 0) {
        return;
      }

      const length = Number(contentLengthPattern.exec(this.buffer.subarray(0, headerEnd).toString())?.[1] ?? 0);
      const bodyStart = headerEnd + headerSeparator.length;
      const isComplete = this.buffer.length >= bodyStart + length;
      if (!isComplete) {
        return;
      }

      const text = this.buffer.subarray(bodyStart, bodyStart + length).toString("utf8");
      this.buffer = this.buffer.subarray(bodyStart + length);
      this.handle(messageSchema.parse(JSON.parse(text)));
    }
  }

  private handle(message: Message): void {
    const resolve = message.id === undefined ? undefined : this.pending.get(message.id);
    const isResponse = resolve !== undefined && message.method === undefined;
    if (isResponse) {
      this.pending.delete(message.id ?? 0);
      resolve(message.result);
      return;
    }

    if (message.method === "textDocument/publishDiagnostics") {
      const published = publishedSchema.parse(message.params);
      this.diagnostics.set(uriKey(published.uri), published.diagnostics);
      this.lastPublish = Date.now();
      return;
    }

    const isServerRequest = message.id !== undefined && message.method !== undefined;
    if (!isServerRequest) {
      return;
    }

    // The editor's settings, and no answer to the rest (progress, watchers).
    const isConfiguration = message.method === "workspace/configuration";
    const result = isConfiguration
      ? configurationRequestSchema.parse(message.params).items.map(item => configuration.get(item.section ?? "") ?? {})
      : null;
    this.send({
      id: message.id,
      result
    });
  }
}
