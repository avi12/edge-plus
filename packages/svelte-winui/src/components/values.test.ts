import {
  attachedList,
  cornerSides,
  gridLengthParts,
  GridUnit,
  parseColor,
  thicknessSides
} from "./values.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

test("a number is every side of a thickness", () => {
  assert.deepEqual(thicknessSides(8), [8, 8, 8, 8]);
});

test("a pair is left-right and top-bottom, as in XAML", () => {
  assert.deepEqual(thicknessSides([8, 4]), [8, 4, 8, 4]);
});

test("four numbers are left, top, right, bottom", () => {
  assert.deepEqual(thicknessSides([1, 2, 3, 4]), [1, 2, 3, 4]);
});

test("a string without commas is CSS order, top right bottom left", () => {
  assert.deepEqual(thicknessSides("12px 16px"), [16, 12, 16, 12]);
  assert.deepEqual(thicknessSides("1 2 3"), [2, 1, 2, 3]);
  assert.deepEqual(thicknessSides("1 2 3 4"), [4, 1, 2, 3]);
});

test("a string with commas is XAML order", () => {
  assert.deepEqual(thicknessSides("1,2,3,4"), [1, 2, 3, 4]);
  assert.deepEqual(thicknessSides("8,4"), [8, 4, 8, 4]);
});

test("a thickness that is not a length fails with the text", () => {
  assert.throws(() => thicknessSides("wide"), /"wide" is not a length/u);
});

test("a corner radius reads like a thickness, in corner order", () => {
  assert.deepEqual(cornerSides(6), [6, 6, 6, 6]);
  assert.deepEqual(cornerSides([1, 2, 3, 4]), [1, 2, 3, 4]);
  assert.deepEqual(cornerSides("4,8"), [4, 8, 4, 8]);
});

test("a grid length is pixels, Auto or a star weight", () => {
  assert.deepEqual(gridLengthParts(120), {
    unit: GridUnit.Pixel,
    value: 120
  });
  assert.deepEqual(gridLengthParts("Auto"), {
    unit: GridUnit.Auto,
    value: 1
  });
  assert.deepEqual(gridLengthParts("*"), {
    unit: GridUnit.Star,
    value: 1
  });
  assert.deepEqual(gridLengthParts("2*"), {
    unit: GridUnit.Star,
    value: 2
  });
});

test("a color is #rgb, #rrggbb or #aarrggbb", () => {
  assert.deepEqual(parseColor("#fff"), {
    a: 255,
    r: 255,
    g: 255,
    b: 255
  });
  assert.deepEqual(parseColor("#0a141e"), {
    a: 255,
    r: 10,
    g: 20,
    b: 30
  });
  assert.deepEqual(parseColor("#800a141e"), {
    a: 128,
    r: 10,
    g: 20,
    b: 30
  });
  assert.equal(parseColor("CardBackgroundFillColorDefaultBrush"), null);
});

test("attached takes one entry or a list of them", () => {
  const owner = {};
  assert.deepEqual(attachedList([owner, "Row", 1]), [[owner, "Row", 1]]);
  assert.deepEqual(attachedList([[owner, "Row", 1], [owner, "Column", 2]]), [[owner, "Row", 1], [owner, "Column", 2]]);
});
