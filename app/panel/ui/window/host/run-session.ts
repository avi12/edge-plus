import type { MessagePort } from "node:worker_threads";

import { runWindow } from "winui-svelte/window";

import { startSession } from "./session.ts";

/** Starts WinUI on this thread and the session once it is up; settles when the window closes. */
export function runSession({ link, onStart }: { link: MessagePort; onStart?: () => void }) {
  return runWindow({
    onStart() {
      onStart?.();
      startSession(link);
    }
  });
}
