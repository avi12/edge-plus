import { MessagePort, workerData } from "node:worker_threads";
import { createWindowModuleRunner } from "winui-svelte/runner";
import { runOnUiThread } from "winui-svelte/thread";
import type * as WindowModule from "winui-svelte/window";
import { z } from "zod";

// The window: WinUI's thread. Its modules come from the host's dev server, and
// a timer takes the server's hot updates in, since WinUI holds this thread.

const receiveMilliseconds = 30;
const windowSize = {
  width: 480,
  height: 320
};

const workerDataSchema = z.object({ modules: z.instanceof(MessagePort) });

await runOnUiThread(async () => {
  const parsedData = workerDataSchema.safeParse(workerData);
  if (!parsedData.success) {
    throw new Error("the host passed no port to its dev server");
  }

  // Everything goes through the runner, so the window and the components
  // share one renderer.
  const { runner, receive } = createWindowModuleRunner(parsedData.data.modules);
  const { createWindow, runWindow, startTimer } = await runner.import<typeof WindowModule>("winui-svelte/window");
  const { default: App } = await runner.import("/App.svelte");
  await runWindow({
    onStart() {
      startTimer({
        milliseconds: receiveMilliseconds,
        onTick: receive
      });
      createWindow({
        component: App,
        props: {},
        title: "Hello",
        size: windowSize,
        // Set for an unattended run: the window opens without taking focus.
        isBackground: process.env["WINUI_SVELTE_BACKGROUND"] === "1"
      });
    }
  });
});
