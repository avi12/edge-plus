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

export function createStdioTransport(streams: StdioStreams = {
  input: process.stdin,
  output: process.stdout
}): Transport {
  return {
    send(value) {
      streams.output.write(`${JSON.stringify(value)}\n`);
    },
    listen(listener: TransportListener) {
      const lines = createInterface({ input: streams.input });
      lines.on("line", line => {
        const isBlank = line.trim().length === 0;
        if (isBlank) {
          return;
        }

        try {
          listener.message(JSON.parse(line));
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
  function toStderr(...values: unknown[]): void {
    console.error(...values);
  }
  console.log = toStderr;
  console.info = toStderr;
  console.debug = toStderr;
}
