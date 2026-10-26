import { registerHooks } from "node:module";
import { parentPort, receiveMessageOnPort } from "node:worker_threads";
import { svelteHooks } from "winui-svelte/testing";
import { runOnUiThread } from "winui-svelte/thread";

// The UI thread of the scheduling experiment: the package's window start
// (startScheduled, then Application.create), a window holding probe.svelte, and
// three ways to change its state from outside Svelte.

const timerTriggerMilliseconds = 1000;
const idleTickMilliseconds = 400;
const inboxMilliseconds = 30;
const afterMilliseconds = 100;
const messageCount = 200;
const timerCount = 100;

await runOnUiThread(async () => {
  registerHooks(svelteHooks());
  await import("winui-svelte/components");
  const { createWindow, runWindow, startTimer } = await import("winui-svelte/window");
  const { default: Probe } = await import("./Probe.svelte");
  const { probe } = await import("./probe-state.svelte.ts");
  const { look, record, watch } = await import("./watch.ts");

  await runWindow({
    onStart() {
      createWindow({
        component: Probe,
        props: {},
        title: "scheduling-probe",
        isBackground: true
      });
      record("window shown", "");

      // Trigger B: a DispatcherQueue timer callback.
      startTimer({
        milliseconds: timerTriggerMilliseconds,
        isRepeating: false,
        onTick() {
          probe.count = timerCount;
          watch("timer");
          startTimer({
            milliseconds: afterMilliseconds,
            isRepeating: false,
            onTick: () => look("timer: +100ms (new timer)")
          });
        }
      });

      // Trigger C: a message from the host, taken off the port by a timer.
      startTimer({
        milliseconds: inboxMilliseconds,
        onTick() {
          const received = parentPort === null ? undefined : receiveMessageOnPort(parentPort);
          if (received !== undefined) {
            probe.count = messageCount;
            watch("host message");
          }
        }
      });

      // What is on screen when nothing of ours is running.
      startTimer({
        milliseconds: idleTickMilliseconds,
        onTick: () => look("idle tick")
      });
    }
  });
});
