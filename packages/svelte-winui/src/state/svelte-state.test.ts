import "./test-support/svelte-runes.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// The Svelte tools src/state/README.md points to, each run for real.

const { flushSync } = await import("svelte");
const { SvelteMap } = await import("svelte/reactivity");
const { fromStore } = await import("svelte/store");
const { writable } = await import("svelte/store");
const { Tallies } = await import("../../examples/counter/tallies.svelte.ts");
const { track } = await import("./test-support/track.svelte.ts");

test("a class holding $state, $derived and a SvelteSet updates what reads it", () => {
  const tallies = new Tallies();
  const doubled = track(() => tallies.doubled);
  const used = track(() => [...tallies.buttonsUsed].join(","));
  tallies.bump("a");
  tallies.bump("b");
  flushSync();
  assert.deepEqual(doubled.values, [0, 4]);
  assert.deepEqual(used.values, ["", "a,b"]);
  doubled.stop();
  used.stop();
});

test("a SvelteMap signals per key", () => {
  const prices = new SvelteMap([["tea", 3], ["coffee", 4]]);
  const tea = track(() => prices.get("tea"));
  const coffee = track(() => prices.get("coffee"));
  prices.set("coffee", 5);
  flushSync();
  assert.deepEqual(tea.values, [3]);
  assert.deepEqual(coffee.values, [4, 5]);
  tea.stop();
  coffee.stop();
});

test("a store from a library is read in runes through fromStore", () => {
  const store = writable(1);
  const bridged = fromStore(store);
  const seen = track(() => bridged.current);
  store.set(2);
  flushSync();
  assert.deepEqual(seen.values, [1, 2]);
  seen.stop();
});
