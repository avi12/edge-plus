import { DispatcherQueue } from "../winui.ts";

// Timers on the UI thread's dispatcher. WinUI holds this thread, so Node's own
// timers and message events never fire here; whatever the window needs to do
// on a clock (take in a host's messages, a hot update) runs on one of these.

const ticksPerMillisecond = 10_000n;

type Timer = ReturnType<DispatcherQueue["createTimer"]>;

// Held for as long as they run: a timer whose wrapper is collected stops.
const heldTimers = new Set<Timer>();

export interface TimerOptions {
  milliseconds: number;
  onTick: () => void;
  // Default true; false runs `onTick` once.
  isRepeating?: boolean;
}

// Starts a timer; call it on the UI thread. Returns what stops it.
export function startTimer(options: TimerOptions): () => void {
  const timer = DispatcherQueue.getForCurrentThread().createTimer();
  timer.interval = { duration: BigInt(options.milliseconds) * ticksPerMillisecond };
  timer.isRepeating = options.isRepeating ?? true;
  timer.onTick(options.onTick);
  heldTimers.add(timer);
  timer.start();
  return () => {
    timer.stop();
    heldTimers.delete(timer);
  };
}
