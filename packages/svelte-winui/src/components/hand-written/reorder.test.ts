import { MoveDirection, movedIndex, sameOrder } from "./reorder.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

const order = ["a", "b", "c"];

test("a key moves one place up or down", () => {
  assert.equal(movedIndex(order, "b", MoveDirection.Up), 0);
  assert.equal(movedIndex(order, "b", MoveDirection.Down), 2);
});

test("a key at an end cannot move past it, and an unknown key does not move", () => {
  assert.equal(movedIndex(order, "a", MoveDirection.Up), null);
  assert.equal(movedIndex(order, "c", MoveDirection.Down), null);
  assert.equal(movedIndex(order, "x", MoveDirection.Down), null);
});

test("two orders are the same when they list the same keys in the same places", () => {
  assert.ok(sameOrder(order, ["a", "b", "c"]));
  assert.ok(!sameOrder(order, ["a", "c", "b"]));
  assert.ok(!sameOrder(order, ["a", "b"]));
});
