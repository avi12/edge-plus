import assert from "node:assert/strict";
import { test } from "node:test";
import type { Rect } from "./layout.ts";
import {
  Axis,
  fits,
  fromTemplate,
  fromWindowBounds,
  layout,
  pane,
  panes,
  remove,
  resize,
  slotCount,
  split,
  toWindowBounds
} from "./layout.ts";

const REGION: Rect = { left: 0, top: 0, width: 1200, height: 800 };
const MINIMUM = { width: 160, height: 160 };

const sideBySide = () => split({ root: pane(1), targetWindowId: 1, newWindowId: 2, axis: Axis.row });

test("splitting the same way adds a sibling that halves the pane", () => {
  const root = split({ root: sideBySide(), targetWindowId: 2, newWindowId: 3, axis: Axis.row });
  assert.deepEqual(panes(root), [1, 2, 3]);
  const rects = layout({ root, region: REGION, gap: 0 });
  assert.equal(rects.get(1)!.width, 600);
  assert.equal(rects.get(2)!.width, 300);
  assert.equal(rects.get(3)!.left, 900);
});

test("splitting across nests a split in the pane's place", () => {
  const root = split({ root: sideBySide(), targetWindowId: 2, newWindowId: 3, axis: Axis.column });
  const rects = layout({ root, region: REGION, gap: 0 });
  assert.deepEqual(rects.get(2), { left: 600, top: 0, width: 600, height: 400 });
  assert.deepEqual(rects.get(3), { left: 600, top: 400, width: 600, height: 400 });
});

test("panes always cover the region exactly, gaps included", () => {
  const root = split({ root: sideBySide(), targetWindowId: 1, newWindowId: 3, axis: Axis.row });
  const rects = layout({ root, region: { left: 7, top: 0, width: 1001, height: 500 }, gap: 6 });
  const last = rects.get(2)!;
  assert.equal(last.left + last.width, 7 + 1001);
  assert.equal(rects.get(3)!.left, rects.get(1)!.left + rects.get(1)!.width + 6);
});

test("removing a pane collapses its split and folds same-axis levels", () => {
  const stacked = split({ root: pane(1), targetWindowId: 1, newWindowId: 2, axis: Axis.column });
  const nested = split({ root: stacked, targetWindowId: 2, newWindowId: 3, axis: Axis.row });
  const root = remove({ root: nested, windowId: 3 })!;
  assert.deepEqual(panes(root), [1, 2]);
  assert.equal(remove({ root: pane(1), windowId: 1 }), null);
});

test("dragging an inner edge moves only the boundary it sits on", () => {
  const next = resize({
    root: sideBySide(), region: REGION, gap: 0, windowId: 1, wanted: { left: 0, top: 0, width: 400, height: 800 }, minimum: MINIMUM
  });
  const rects = layout({ root: next.root, region: next.region, gap: 0 });
  assert.equal(rects.get(1)!.width, 400);
  assert.deepEqual(rects.get(2), { left: 400, top: 0, width: 800, height: 800 });
  assert.deepEqual(next.region, REGION);
});

test("dragging an outer edge resizes the whole group", () => {
  const next = resize({
    root: sideBySide(), region: REGION, gap: 0, windowId: 2, wanted: { left: 600, top: 0, width: 500, height: 700 }, minimum: MINIMUM
  });
  assert.deepEqual(next.region, { left: 0, top: 0, width: 1100, height: 700 });
});

test("moving a pane without resizing carries the group along", () => {
  const stacked = split({ root: pane(1), targetWindowId: 1, newWindowId: 2, axis: Axis.column });
  const next = resize({
    root: stacked, region: REGION, gap: 0, windowId: 2, wanted: { left: 50, top: 430, width: 1200, height: 400 }, minimum: MINIMUM
  });
  assert.deepEqual(next.region, { left: 50, top: 30, width: 1200, height: 800 });
});

test("no pane is squeezed below the minimum", () => {
  const next = resize({
    root: sideBySide(), region: REGION, gap: 0, windowId: 1, wanted: { left: 0, top: 0, width: 1190, height: 800 }, minimum: MINIMUM
  });
  assert.ok(layout({ root: next.root, region: next.region, gap: 0 }).get(2)!.width >= 160);
});

test("templates build equal-weight trees in slot order", () => {
  const template = {
    axis: Axis.row,
    children: [{ axis: Axis.column, children: [0, 2] }, { axis: Axis.column, children: [1, 3] }]
  };
  assert.equal(slotCount(template), 4);
  const rects = layout({ root: fromTemplate({ template, windowIds: [10, 11, 12, 13] }), region: REGION, gap: 0 });
  assert.deepEqual(rects.get(13), { left: 600, top: 400, width: 600, height: 400 });
});

test("panes grow by the invisible border only where they meet a neighbour", () => {
  const rects = layout({ root: sideBySide(), region: REGION, gap: 0 });
  assert.deepEqual(toWindowBounds({ rect: rects.get(1)!, region: REGION, border: 7 }), { left: 0, top: 0, width: 607, height: 800 });
  const bounds = toWindowBounds({ rect: rects.get(2)!, region: REGION, border: 7 });
  assert.deepEqual(bounds, { left: 593, top: 0, width: 607, height: 800 });
  assert.deepEqual(fromWindowBounds({ bounds, rect: rects.get(2)!, region: REGION, border: 7 }), rects.get(2));
});

test("a layout fits only when every pane reaches the minimum", () => {
  const columns = { axis: Axis.row, children: [0, 1, 2] };
  const root = fromTemplate({ template: columns, windowIds: [1, 2, 3] });
  assert.equal(fits({ root, region: REGION, gap: 0, minimum: { width: 400, height: 150 } }), true);
  assert.equal(fits({ root, region: REGION, gap: 0, minimum: { width: 516, height: 150 } }), false);
});
