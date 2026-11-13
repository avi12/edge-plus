import { strict as assert } from "node:assert";
import { describe, it } from "node:test";

import { clampedOffset, movedIds, offsetBounds, slideOffsets, slotOffset, targetIndex } from "./reorder-math.ts";

const HEIGHTS = [40, 50, 60];

describe("where a dragged row lands", () => {
  it("stays put until its leading edge passes a neighbour's middle", () => {
    assert.equal(
      targetIndex({
        heights: HEIGHTS,
        index: 0,
        offset: 0
      }),
      0
    );
    assert.equal(
      targetIndex({
        heights: HEIGHTS,
        index: 0,
        offset: 24
      }),
      0
    );
    assert.equal(
      targetIndex({
        heights: HEIGHTS,
        index: 0,
        offset: 26
      }),
      1
    );
  });

  it("passes every neighbour on a long drag, in either direction", () => {
    assert.equal(
      targetIndex({
        heights: HEIGHTS,
        index: 0,
        offset: 500
      }),
      2
    );
    assert.equal(
      targetIndex({
        heights: HEIGHTS,
        index: 2,
        offset: -500
      }),
      0
    );
  });

  it("is kept within the list", () => {
    assert.deepEqual(
      offsetBounds({
        heights: HEIGHTS,
        index: 1
      }),
      {
        min: -40,
        max: 60
      }
    );
    assert.equal(
      clampedOffset({
        heights: HEIGHTS,
        index: 1,
        offset: -90
      }),
      -40
    );
    assert.equal(
      clampedOffset({
        heights: HEIGHTS,
        index: 1,
        offset: 90
      }),
      60
    );
  });
});

describe("how the other rows slide aside", () => {
  it("rows passed going down slide up by the dragged height", () => {
    assert.deepEqual(
      slideOffsets({
        heights: HEIGHTS,
        index: 0,
        target: 2
      }),
      [0, -40, -40]
    );
  });

  it("rows passed going up slide down by the dragged height", () => {
    assert.deepEqual(
      slideOffsets({
        heights: HEIGHTS,
        index: 2,
        target: 0
      }),
      [60, 60, 0]
    );
  });

  it("nothing slides when the row is back in its own place", () => {
    assert.deepEqual(
      slideOffsets({
        heights: HEIGHTS,
        index: 1,
        target: 1
      }),
      [0, 0, 0]
    );
  });
});

describe("the slot the dragged row settles in", () => {
  it("is the heights of the rows it passed", () => {
    assert.equal(
      slotOffset({
        heights: HEIGHTS,
        index: 0,
        target: 2
      }),
      110
    );
    assert.equal(
      slotOffset({
        heights: HEIGHTS,
        index: 2,
        target: 0
      }),
      -90
    );
    assert.equal(
      slotOffset({
        heights: HEIGHTS,
        index: 1,
        target: 1
      }),
      0
    );
  });
});

describe("the order that results", () => {
  it("moves one id and keeps the rest in order", () => {
    assert.deepEqual(
      movedIds({
        ids: ["a", "b", "c"],
        index: 0,
        target: 2
      }),
      ["b", "c", "a"]
    );
    assert.deepEqual(
      movedIds({
        ids: ["a", "b", "c"],
        index: 2,
        target: 0
      }),
      ["c", "a", "b"]
    );
  });
});
