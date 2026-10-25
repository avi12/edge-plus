import { appendFileSync } from "node:fs";

// What the experiment saw, one line per observation, written with a plain
// synchronous append because a worker's stdout waits for the thread that
// WinUI is holding.
const logPath = process.env["SCHEDULING_LOG"] ?? "scheduling.log";
const clockStart = performance.now();
const timeWidth = 6;
const labelWidth = 28;
let readText: () => string = () => "";

export function record(label: string, text: string): void {
  const elapsed = (performance.now() - clockStart).toFixed(0).padStart(timeWidth);
  appendFileSync(logPath, `${elapsed}ms  ${label.padEnd(labelWidth)} ${text}\n`);
}

// What the observed control shows right now.
export function observe(read: () => string): void {
  readText = read;
}

export function look(label: string): void {
  record(label, readText());
}

// Called right after a state change made by `trigger`: records the control's
// text now, after the next microtask, and after a promise continuation.
export function watch(trigger: string): void {
  record(`${trigger}: sync`, readText());
  queueMicrotask(() => record(`${trigger}: queueMicrotask`, readText()));
  void Promise.resolve().then(() => record(`${trigger}: promise.then`, readText()));
}
