import { flushSync } from "svelte";

// Runs `read` inside an effect and records what it returned each time the
// effect ran: how a test sees which bindings a change reached.
export function track<Value>(read: () => Value): {
  values: Value[];
  stop(): void;
} {
  const values: Value[] = [];
  const stop = $effect.root(() => {
    $effect(() => {
      values.push(read());
    });
  });
  flushSync();
  return {
    values,
    stop
  };
}
