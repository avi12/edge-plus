import { jsonSchema } from "../../src/messaging/json.ts";
import type { Json } from "../../src/messaging/json.ts";
import { z } from "zod";

// What an example's entry module and the harness (launch.ts, worker.ts) say to
// each other. The entry exports `start`, which the harness calls on the UI
// thread once WinUI is up; the harness carries JSON lines between the window
// and whoever started it (a test, a script).

// One command: a JSON object.
export type ExampleCommand = Record<string, Json>;

export const exampleCommandSchema = z.record(z.string(), jsonSchema);

export interface ExampleHost {
  // A line for whoever started the example.
  report(message: object): void;
  // Commands from whoever started the example (one JSON object per line on
  // the process's stdin), taken off the port by a dispatcher timer.
  onCommand(handler: (command: ExampleCommand) => void): void;
  // Flags from the command line (`--rows 1000` is { rows: "1000" }).
  readonly options: Readonly<Record<string, string>>;
}

interface ExampleModule {
  start(host: ExampleHost): void;
}

export const exampleModuleSchema = z.object({
  start: z.custom<ExampleModule["start"]>(value => typeof value === "function")
});
