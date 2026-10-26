import type { KeyBox, KeyVector } from "./key-vector.ts";
import { duplicateKey, syncKeyVector } from "./key-vector.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

// Keys held as {text}, so a vector entry is not its key's string: a sync that
// compared the wrong thing would show.
interface Held {
  text: string;
}

const codec: KeyBox<Held> = {
  box: key => ({ text: key }),
  unbox: held => held.text
};

// Counts every change, since each one is a layout and a transition in WinUI.
class RecordedVector implements KeyVector<Held> {
  readonly held: Held[] = [];
  readonly changes: string[] = [];

  get size(): number {
    return this.held.length;
  }

  getAt(index: number): Held {
    return this.held[index];
  }

  removeAt(index: number): void {
    this.changes.push(`remove ${index}`);
    this.held.splice(index, 1);
  }

  insertAt(index: number, held: Held): void {
    this.changes.push(`insert ${index}`);
    this.held.splice(index, 0, held);
  }

  replaceAll(held: Held[]): void {
    this.changes.push(`replaceAll ${held.length}`);
    this.held.splice(0, this.held.length, ...held);
  }

  keys(): string {
    return this.held.map(codec.unbox).join(",");
  }
}

test("an empty vector is filled by one reset", () => {
  const vector = new RecordedVector();
  syncKeyVector(vector, codec, ["a", "b", "c"]);
  assert.equal(vector.keys(), "a,b,c");
  assert.deepEqual(vector.changes, ["replaceAll 3"]);
});

test("a vector with keys changes only the keys that came or went", () => {
  const vector = new RecordedVector();
  syncKeyVector(vector, codec, ["a", "b", "c", "d"]);
  vector.changes.length = 0;
  syncKeyVector(vector, codec, ["a", "c", "d", "e"]);
  assert.equal(vector.keys(), "a,c,d,e");
  assert.deepEqual(vector.changes, ["remove 1", "insert 3"]);
});

test("the same keys change nothing", () => {
  const vector = new RecordedVector();
  syncKeyVector(vector, codec, ["a", "b"]);
  vector.changes.length = 0;
  syncKeyVector(vector, codec, ["a", "b"]);
  assert.deepEqual(vector.changes, []);
});

test("an emptied list leaves the vector empty", () => {
  const vector = new RecordedVector();
  syncKeyVector(vector, codec, ["a", "b"]);
  syncKeyVector(vector, codec, []);
  assert.equal(vector.size, 0);
});

test("a duplicate key is found", () => {
  assert.equal(duplicateKey(["a", "b", "a"]), "a");
  assert.equal(duplicateKey(["a", "b"]), undefined);
});
