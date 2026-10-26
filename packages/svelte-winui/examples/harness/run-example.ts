import * as bindings from "../../src/components/generated/bindings.ts";
import { registerControls } from "../../src/renderer/host/host.ts";
import { runWindow } from "../../src/window/index.ts";
import { DispatcherQueue } from "../../src/winui.ts";
import type { ExampleCommand, ExampleHost } from "./contract.ts";
import { exampleCommandSchema, exampleModuleSchema } from "./contract.ts";
import type { MessagePort } from "node:worker_threads";
import { receiveMessageOnPort } from "node:worker_threads";

// Loaded through the module server, like an app's own modules: the example's
// start() runs on the UI thread once WinUI is up, and the host's commands are
// taken off the port by a dispatcher timer (WinUI holds the thread, so the
// port's events never fire).

const commandMilliseconds = 30;
const ticksPerMillisecond = 10_000n;

// Held for as long as the window runs: a timer whose wrapper is collected
// stops.
const heldTimers = new Set<ReturnType<ReturnType<typeof DispatcherQueue.getForCurrentThread>["createTimer"]>>();

export type RunExample = (entry: string, link: MessagePort, options: Record<string, string>) => Promise<void>;

export async function runExample(entry: string, link: MessagePort, options: Record<string, string>): Promise<void> {
  const parsedExample = exampleModuleSchema.safeParse(await import(/* @vite-ignore */ entry));
  if (!parsedExample.success) {
    throw new Error(`${entry} exports no start()`);
  }

  const { start } = parsedExample.data;
  const handlers: Array<(command: ExampleCommand) => void> = [];
  const host: ExampleHost = {
    report: message => link.postMessage(message),
    onCommand(handler) {
      handlers.push(handler);
    },
    options
  };
  await runWindow({
    onStart() {
      registerControls(bindings);
      start(host);
      const timer = DispatcherQueue.getForCurrentThread().createTimer();
      timer.interval = { duration: BigInt(commandMilliseconds) * ticksPerMillisecond };
      timer.isRepeating = true;
      timer.onTick(() => {
        let received = receiveMessageOnPort(link);
        while (received !== undefined) {
          const parsedCommand = exampleCommandSchema.safeParse(received.message);
          if (parsedCommand.success) {
            handlers.forEach(handler => handler(parsedCommand.data));
          }

          received = receiveMessageOnPort(link);
        }
      });
      timer.start();
      heldTimers.add(timer);
    }
  });
}
