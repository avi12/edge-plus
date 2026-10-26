import type { LiveRow } from "./live-rows.ts";
import { LiveRows } from "./live-rows.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// A row that counts what happened to it, so a leak (a row never ended) and a
// needless rebuild (a row made again for the same key) are both visible.
class Tally {
  made = 0;
  ended = 0;

  get live(): number {
    return this.made - this.ended;
  }

  row(shown: string[]): LiveRow<string> {
    this.made++;
    return {
      update: item => shown.push(item),
      unmount: () => {
        this.ended++;
      }
    };
  }
}

test("a container keeps its row while it shows the same key, with the item it has now", () => {
  const tally = new Tally();
  const shown: string[] = [];
  const rows = new LiveRows<string, string>();
  rows.fill("c1", "container 1", "a", "first a", () => tally.row(shown));
  rows.fill("c1", "container 1", "a", "second a", () => tally.row(shown));
  assert.equal(tally.made, 1);
  assert.deepEqual(shown, ["second a"]);
  assert.equal(rows.count, 1);
});

test("a container recycled to another key ends its row and gets a new one", () => {
  const tally = new Tally();
  const rows = new LiveRows<string, string>();
  rows.fill("c1", "container 1", "a", "a", () => tally.row([]));
  rows.fill("c1", "container 1", "b", "b", () => tally.row([]));
  assert.equal(tally.made, 2);
  assert.equal(tally.ended, 1);
  assert.equal(rows.count, 1);
});

test("a container in the recycle queue ends its row, and releasing twice is harmless", () => {
  const tally = new Tally();
  const rows = new LiveRows<string, string>();
  rows.fill("c1", "container 1", "a", "a", () => tally.row([]));
  rows.release("c1");
  rows.release("c1");
  rows.release("never seen");
  assert.equal(tally.live, 0);
  assert.equal(rows.count, 0);
});

test("scrolling through a long list never holds more rows than containers", () => {
  const tally = new Tally();
  const rows = new LiveRows<string, string>();
  const containers = 12;
  let mostLive = 0;
  for (let index = 0; index < 1000; index++) {
    const container = `c${index % containers}`;
    rows.release(container);
    rows.fill(container, container, `key ${index}`, `item ${index}`, () => tally.row([]));
    mostLive = Math.max(mostLive, tally.live);
  }

  assert.equal(tally.made, 1000);
  assert.equal(mostLive, containers);
  rows.clear();
  assert.equal(tally.live, 0);
});

test("a changed item reaches the live rows of its key only", () => {
  const rows = new LiveRows<string, string>();
  const shownA: string[] = [];
  const shownB: string[] = [];
  const tally = new Tally();
  rows.fill("c1", "container 1", "a", "a", () => tally.row(shownA));
  rows.fill("c2", "container 2", "b", "b", () => tally.row(shownB));
  rows.refresh(key => (key === "a" ? "a, renamed" : undefined));
  assert.deepEqual(shownA, ["a, renamed"]);
  assert.deepEqual(shownB, []);
  assert.deepEqual(rows.containers(), ["container 1", "container 2"]);
});
