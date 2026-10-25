import {
  parseColor,
  parseLength,
  parseMilliseconds,
  parseNumber,
  parsePercent,
  parseVariable,
  splitCommas,
  splitSlash,
  splitWords
} from "./values.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

test("lengths are px, rem (16px) or a bare zero", () => {
  assert.equal(parseLength("12px"), 12);
  assert.equal(parseLength("-4.5px"), -4.5);
  assert.equal(parseLength("1.5rem"), 24);
  assert.equal(parseLength("0"), 0);
  assert.equal(parseLength("12"), null);
  assert.equal(parseLength("50%"), null);
  assert.equal(parseLength("2em"), null);
  assert.equal(parseLength("calc(1px + 2px)"), null);
});

test("colors are spelled the way XAML takes them", () => {
  assert.equal(parseColor("#0af"), "#00AAFF");
  assert.equal(parseColor("#00aaff"), "#00AAFF");
  assert.equal(parseColor("#00aaff80"), "#8000AAFF");
  assert.equal(parseColor("#0af8"), "#8800AAFF");
  assert.equal(parseColor("rgb(0, 170, 255)"), "#00AAFF");
  assert.equal(parseColor("rgb(0 170 255 / 50%)"), "#8000AAFF");
  assert.equal(parseColor("rgba(255, 0, 0, 0.5)"), "#80FF0000");
  assert.equal(parseColor("hsl(120, 100%, 25%)"), "#008000");
  assert.equal(parseColor("RebeccaPurple"), "#663399");
  assert.equal(parseColor("transparent"), "#00FFFFFF");
  assert.equal(parseColor("currentcolor"), null);
  assert.equal(parseColor("#12"), null);
  assert.equal(parseColor("nonsense"), null);
});

test("numbers, percentages and times", () => {
  assert.equal(parseNumber("0.5"), 0.5);
  assert.equal(parseNumber("1px"), null);
  assert.equal(parsePercent("50%"), 0.5);
  assert.equal(parseMilliseconds("200ms"), 200);
  assert.equal(parseMilliseconds("0.2s"), 200);
  assert.equal(parseMilliseconds("200"), null);
});

test("var() reads its name and fallback", () => {
  assert.deepEqual(parseVariable("var(--a-b)"), {
    name: "--a-b",
    fallback: null
  });
  assert.deepEqual(parseVariable("var( --a , #fff )"), {
    name: "--a",
    fallback: "#fff"
  });
  assert.equal(parseVariable("var(--a) 4px"), null);
  assert.equal(parseVariable("12px"), null);
});

test("lists split outside parentheses and strings", () => {
  assert.deepEqual(splitWords("1px solid rgb(0 0 0 / 50%)"), ["1px", "solid", "rgb(0 0 0 / 50%)"]);
  assert.deepEqual(splitCommas("\"A, B\", serif, rgb(0, 0, 0)"), ["\"A, B\"", "serif", "rgb(0, 0, 0)"]);
  assert.deepEqual(splitSlash("14px/20px"), ["14px", "20px"]);
  assert.deepEqual(splitSlash("2 / span 2"), ["2", "span 2"]);
});
