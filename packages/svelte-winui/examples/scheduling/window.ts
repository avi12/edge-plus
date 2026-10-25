import { roInitialize } from "@microsoft/dynwinrt";
import { registerHooks } from "node:module";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { parentPort, receiveMessageOnPort } from "node:worker_threads";

// The UI thread of the scheduling experiment: the same start as Edge+'s
// session.ts (startScheduled, then Application.create), a window holding
// probe.svelte, and three ways to change its state from outside Svelte.

const singleThreadedApartment = 0;
const timerTriggerMilliseconds = 1000;
const idleTickMilliseconds = 400;
const inboxMilliseconds = 30;
const afterMilliseconds = 100;
const messageCount = 200;
const timerCount = 100;
const panelUi = path.join(import.meta.dirname, "..", "..", "..", "..", "app", "panel-tauri", "ui", "window");

function panelModule(name: string): string {
  return pathToFileURL(path.join(panelUi, name)).href;
}

roInitialize(singleThreadedApartment);
const { svelteHooks } = await import(panelModule("svelte-loader.ts"));
registerHooks(svelteHooks());
const { milliseconds } = await import(panelModule("layout.ts"));
const { Application, DispatcherQueue, Window } = await import(panelModule("winui.ts"));
const { default: renderer, rootControl } = await import(panelModule("svelte-renderer.ts"));
const { mount } = await import("svelte");
const { default: Probe } = await import("./probe.svelte");
const { probe } = await import("./probe-state.svelte.ts");
const { look, record, watch } = await import("./watch.ts");

const held = new Set<unknown>();

function every(count: number, callback: () => void): void {
  const timer = DispatcherQueue.getForCurrentThread().createTimer();
  timer.interval = milliseconds(count);
  timer.isRepeating = true;
  timer.onTick(callback);
  timer.start();
  held.add(timer);
}

function once(count: number, callback: () => void): void {
  const timer = DispatcherQueue.getForCurrentThread().createTimer();
  timer.interval = milliseconds(count);
  timer.isRepeating = false;
  timer.onTick(callback);
  timer.start();
  held.add(timer);
}

await Application.startScheduled(() => {
  Application.create(() => {
    const fragment = renderer.createFragment();
    mount(Probe, {
      renderer,
      target: fragment
    });
    const window = new Window();
    window.content = rootControl(fragment);
    window.title = "scheduling-probe";
    window.appWindow.showWithActivation(false);
    record("window shown", "");

    // Trigger B: a DispatcherQueue timer callback.
    once(timerTriggerMilliseconds, () => {
      probe.count = timerCount;
      watch("timer");
      once(afterMilliseconds, () => look("timer: +100ms (new timer)"));
    });

    // Trigger C: a message from the host, taken off the port by a timer.
    every(inboxMilliseconds, () => {
      const received = parentPort === null ? undefined : receiveMessageOnPort(parentPort);
      if (received !== undefined) {
        probe.count = messageCount;
        watch("host message");
      }
    });

    // What is on screen when nothing of ours is running.
    every(idleTickMilliseconds, () => look("idle tick"));
  });
});
process.exit(0);
