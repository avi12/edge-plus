import { roInitialize } from "@microsoft/dynwinrt";
import { registerHooks } from "node:module";
import type { MessagePort } from "node:worker_threads";
import { workerData } from "node:worker_threads";
import { panelWindowModule } from "../bootstrap.ts";

// The counter example's UI thread: Edge+'s window start (startScheduled,
// Application.create) around the package's state layer. The host's messages
// come in through a MessagePort drained by a DispatcherQueue timer, and no
// code here calls flushSync.

const singleThreadedApartment = 0;
const pollMilliseconds = 30;

function isPort(value: unknown): value is MessagePort {
  return typeof value === "object" && value !== null && "postMessage" in value;
}

function portFrom(data: unknown): MessagePort {
  const port = typeof data === "object" && data !== null && "port" in data ? data.port : undefined;
  if (!isPort(port)) {
    throw new Error("the host passed no port");
  }

  return port;
}

roInitialize(singleThreadedApartment);
const { svelteHooks } = await import(panelWindowModule("svelte-loader.ts"));
registerHooks(svelteHooks());
const { milliseconds } = await import(panelWindowModule("layout.ts"));
const { Application, DispatcherQueue, Window } = await import(panelWindowModule("winui.ts"));
const { default: renderer, rootControl } = await import(panelWindowModule("svelte-renderer.ts"));
const { mount } = await import("svelte");
const { default: App } = await import("./app.svelte");
const { createHostChannel } = await import("../../src/state/index.ts");
const { createMessagePortTransport } = await import("../../src/state/transports/message-port.ts");

const port = portFrom(workerData);
// Held for as long as the window runs: a timer whose wrapper is collected stops.
const heldTimers = new Set<unknown>();

await Application.startScheduled(() => {
  Application.create(() => {
    const channel = createHostChannel(createMessagePortTransport(port, {
      poll(drain) {
        const timer = DispatcherQueue.getForCurrentThread().createTimer();
        timer.interval = milliseconds(pollMilliseconds);
        timer.isRepeating = true;
        timer.onTick(drain);
        timer.start();
        heldTimers.add(timer);
        return () => timer.stop();
      }
    }));
    const fragment = renderer.createFragment();
    mount(App, {
      renderer,
      target: fragment,
      props: { channel }
    });
    const window = new Window();
    window.content = rootControl(fragment);
    window.title = "counter-example";
    window.appWindow.showWithActivation(false);
  });
});
process.exit(0);
