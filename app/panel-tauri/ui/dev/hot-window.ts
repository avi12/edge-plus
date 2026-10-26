import type * as Session from "../window/host/session.ts";
import type * as Layout from "../window/style/layout.ts";
import type * as WinUI from "../window/winui.ts";
import type { MessagePort } from "node:worker_threads";
import { createWindowModuleRunner } from "winui-svelte/runner";

// The window run from source: its modules load through the dev server
// (window-modules.ts), and the server's replies and hot updates are taken in
// by a dispatcher timer, since WinUI holds this thread.

const receiveMilliseconds = 30;
// Held for as long as the window runs: a timer whose wrapper is collected
// stops.
const heldTimers = new Set<ReturnType<WinUI.DispatcherQueue["createTimer"]>>();

export async function runHotWindow(link: MessagePort, modules: MessagePort): Promise<void> {
  const { runner, receive } = createWindowModuleRunner(modules);
  const { runSession } = await runner.import<typeof Session>("/window/host/session.ts");
  const { DispatcherQueue } = await runner.import<typeof WinUI>("/window/winui.ts");
  const { milliseconds } = await runner.import<typeof Layout>("/window/style/layout.ts");
  await runSession(link, () => {
    const timer = DispatcherQueue.getForCurrentThread().createTimer();
    timer.interval = milliseconds(receiveMilliseconds);
    timer.isRepeating = true;
    timer.onTick(receive);
    timer.start();
    heldTimers.add(timer);
  });
}
