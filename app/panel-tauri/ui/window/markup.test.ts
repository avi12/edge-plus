import {
  camelCase,
  gridLength,
  GridUnit,
  isNumeric,
  pascalCase,
  thicknessSides
} from "./markup.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

test("kebab-case names read as WinUI's", () => {
  assert.equal(pascalCase("stack-panel"), "StackPanel");
  assert.equal(pascalCase("grid"), "Grid");
  assert.equal(camelCase("vertical-alignment"), "verticalAlignment");
  assert.equal(camelCase("column-span"), "columnSpan");
});

test("a thickness has one, two or four sides, as in XAML", () => {
  assert.deepEqual(thicknessSides("8"), [8, 8, 8, 8]);
  assert.deepEqual(thicknessSides("8, 4"), [8, 4, 8, 4]);
  assert.deepEqual(thicknessSides("0,12,0,0"), [0, 12, 0, 0]);
});

test("a grid length is Auto, a star weight or pixels", () => {
  assert.deepEqual(gridLength("Auto"), {
    unit: GridUnit.Auto,
    value: 1
  });
  assert.deepEqual(gridLength("*"), {
    unit: GridUnit.Star,
    value: 1
  });
  assert.deepEqual(gridLength("2.5*"), {
    unit: GridUnit.Star,
    value: 2.5
  });
  assert.deepEqual(gridLength("48"), {
    unit: GridUnit.Pixel,
    value: 48
  });
});

test("a number is told from a name", () => {
  assert.ok(isNumeric("12"));
  assert.ok(isNumeric("-0.5"));
  assert.ok(!isNumeric(""));
  assert.ok(!isNumeric(" "));
  assert.ok(!isNumeric("Center"));
});
