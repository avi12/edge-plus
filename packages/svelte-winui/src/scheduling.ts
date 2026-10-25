import { DispatcherQueue } from "./winui.ts";
import { flushSync } from "svelte";

// Updates are flushed by hand, and on WinUI's dispatcher. WinUI's loop holds
// the UI thread, so Node's microtasks (where Svelte schedules its own flush)
// have no turn to run in: whoever changes state calls flush(), and state that
// arrives from elsewhere arrives through dispatch().

const flushListeners = new Set<() => void>();

// Svelte's flushSync(), then every onFlush listener.
export function flush(): void {
  flushSync();
  for (const listener of flushListeners) {
    listener();
  }
}

// For parts that must act after Svelte has applied a batch (the CSS runtime).
// Returns what removes the listener.
export function onFlush(listener: () => void): () => void {
  flushListeners.add(listener);
  return () => {
    flushListeners.delete(listener);
  };
}

// Runs `work` on the UI thread's dispatcher queue, then flush(). Called on
// the UI thread (the only one with a queue to get).
export function dispatch(work: () => void): void {
  const isQueued = DispatcherQueue.getForCurrentThread().tryEnqueue(() => {
    work();
    flush();
  });
  if (!isQueued) {
    throw new Error("the UI thread's dispatcher queue is shutting down");
  }
}
