import assert from "node:assert/strict";
import { test } from "node:test";
import type { Pane } from "./layout.ts";
import { Axis, dividers, fromTemplate, layout, moveDivider, panes, remove, setUrl, slotCount, split } from "./layout.ts";

const pane = (id: string): Pane => ({ id, url: `https://example.com/${id}` });

const sideBySide = () => split({ root: pane("a"), targetId: "a", newPane: pane("b"), axis: Axis.row });

test("splitting the same way adds a sibling that halves the pane", () => {
  const root = split({ root: sideBySide(), targetId: "b", newPane: pane("c"), axis: Axis.row });
  assert.deepEqual(panes(root).map(({ id }) => id), ["a", "b", "c"]);
  const rects = layout({ root });
  assert.equal(rects.get("a")!.width, 50);
  assert.equal(rects.get("b")!.width, 25);
  assert.equal(rects.get("c")!.left, 75);
});

test("splitting across nests a split in the pane's place", () => {
  const root = split({ root: sideBySide(), targetId: "b", newPane: pane("c"), axis: Axis.column });
  const rects = layout({ root });
  assert.deepEqual(rects.get("b"), { left: 50, top: 0, width: 50, height: 50 });
  assert.deepEqual(rects.get("c"), { left: 50, top: 50, width: 50, height: 50 });
});

test("removing a pane collapses its split and folds same-axis levels", () => {
  const stacked = split({ root: pane("a"), targetId: "a", newPane: pane("b"), axis: Axis.column });
  const nested = split({ root: stacked, targetId: "b", newPane: pane("c"), axis: Axis.row });
  const root = remove({ root: nested, paneId: "c" })!;
  assert.deepEqual(panes(root).map(({ id }) => id), ["a", "b"]);
  assert.equal(remove({ root: pane("a"), paneId: "a" }), null);
});

test("a pane's address changes without touching its place", () => {
  const root = setUrl({ root: sideBySide(), paneId: "b", url: "https://example.org/" });
  assert.equal(panes(root)[1]!.url, "https://example.org/");
  assert.deepEqual(layout({ root }).get("b"), layout({ root: sideBySide() }).get("b"));
});

test("every neighbouring pair gets a divider at their shared edge", () => {
  const root = split({ root: sideBySide(), targetId: "b", newPane: pane("c"), axis: Axis.column });
  const found = dividers({ root });
  assert.equal(found.length, 2);
  assert.equal(found[0]!.axis, Axis.row);
  assert.equal(found[0]!.at.left, 50);
  assert.equal(found[1]!.axis, Axis.column);
  assert.equal(found[1]!.at.top, 50);
});

test("dragging a divider moves only its pair, and stops short of either edge", () => {
  const root = split({ root: sideBySide(), targetId: "b", newPane: pane("c"), axis: Axis.row });
  const [first] = dividers({ root });
  const moved = moveDivider({ root, divider: first!, fraction: 0.3 });
  const rects = layout({ root: moved });
  assert.equal(Math.round(rects.get("a")!.width), 30);
  assert.equal(Math.round(rects.get("c")!.width), 25);
  const squeezed = layout({ root: moveDivider({ root, divider: first!, fraction: 0 }) });
  assert.ok(squeezed.get("a")!.width > 0);
});

test("templates build equal-weight trees in slot order", () => {
  const template = { axis: Axis.row, children: [{ axis: Axis.column, children: [0, 2] }, { axis: Axis.column, children: [1, 3] }] };
  assert.equal(slotCount(template), 4);
  const rects = layout({ root: fromTemplate({ template, panes: ["a", "b", "c", "d"].map(pane) }) });
  assert.deepEqual(rects.get("d"), { left: 50, top: 50, width: 50, height: 50 });
});
