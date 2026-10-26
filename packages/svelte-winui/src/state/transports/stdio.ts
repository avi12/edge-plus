import type { Frame as MessagingFrame } from "../../messaging/frame.ts";
import { jsonSchema } from "../../messaging/json.ts";
import type { Transport, TransportListener } from "./transport.ts";
import { createInterface } from "node:readline";
import type { Readable, Writable } from "node:stream";

// JSON lines on a process's stdin and stdout: one message per line each way.
// The host starts the window as a child process and uses these two pipes as
// its protocol, so nothing else may be written to stdout (reserveStdout).

export interface StdioStreams {
  input: Readable;
  output: Writable;
}

export function createStdioTransport<Frame = MessagingFrame>(streams: StdioStreams = {
  input: process.stdin,
  output: process.stdout
}): Transport<Frame> {
  return {
    send(frame) {
      streams.output.write(`${JSON.stringify(frame)}\n`);
    },
    listen(listener: TransportListener) {
      const lines = createInterface({ input: streams.input });
      lines.on("line", line => {
        const isBlank = line.trim().length === 0;
        if (isBlank) {
          return;
        }

        try {
          listener.message(jsonSchema.parse(JSON.parse(line)));
        } catch (error) {
          listener.malformed(error instanceof Error ? error.message : String(error));
        }
      });
      lines.on("close", () => listener.closed());
      return () => lines.close();
    }
  };
}

// Sends what the app prints with console.log/info/debug to stderr, so a stray
// log line cannot corrupt the protocol. Call once, first thing in the process.
export function reserveStdout(): void {
  function toStderr(...values: Parameters<typeof console.error>): void {
    console.error(...values);
  }
  console.log = toStderr;
  console.info = toStderr;
  console.debug = toStderr;
}
