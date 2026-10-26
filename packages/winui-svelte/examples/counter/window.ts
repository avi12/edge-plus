import { registerHooks } from "node:module";
import { MessagePort, workerData } from "node:worker_threads";
import { svelteHooks } from "winui-svelte/testing";
import { runOnUiThread } from "winui-svelte/thread";
import { z } from "zod";

// The counter example's UI thread: the package's window start around its state
// layer. The host's messages come in through a MessagePort drained by a
// DispatcherQueue timer, and no code here calls flushSync.

const pollMilliseconds = 30;

const workerDataSchema = z.object({ port: z.instanceof(MessagePort) });

function portFrom(data: object): MessagePort {
  const parsed = workerDataSchema.safeParse(data);
  if (!parsed.success) {
    throw new Error("the host passed no port");
  }

  return parsed.data.port;
}

await runOnUiThread(async () => {
  // .svelte files run from source, through Node's own loader.
  registerHooks(svelteHooks());
  // Registers every WinUI control with the renderer, for the raw tags app.svelte uses.
  await import("winui-svelte/components");
  const { createWindow, runWindow, startTimer } = await import("winui-svelte/window");
  const { createHostChannel, createMessagePortTransport } = await import("winui-svelte/state");
  const { default: App } = await import("./App.svelte");
  const port = portFrom(workerData);
  await runWindow({
    onStart() {
      const channel = createHostChannel(
        createMessagePortTransport(port, {
          poll(drain) {
            return startTimer({
              milliseconds: pollMilliseconds,
              onTick: drain
            });
          }
        })
      );
      createWindow({
        component: App,
        props: {
          channel
        },
        title: "counter-example",
        isBackground: true
      });
    }
  });
});
