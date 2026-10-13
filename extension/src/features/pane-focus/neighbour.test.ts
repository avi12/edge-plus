import assert from "node:assert/strict";
import { test } from "node:test";
import { Axis, fromTemplate, layout } from "../split-panes/layout.ts";
import { Direction, neighbour } from "./neighbour.ts";

// One big pane on the left (1), two stacked on the right (2 over 3)
const RECTS = layout({
  root: fromTemplate({
    template: { axis: Axis.row, children: [0, { axis: Axis.column, children: [1, 2] }] },
    windowIds: [1, 2, 3]
  }),
  region: { left: 0, top: 0, width: 1200, height: 800 },
  gap: 0
});

test("moves to the pane on that side", () => {
  assert.equal(neighbour({ rects: RECTS, fromWindowId: 2, direction: Direction.down }), 3);
  assert.equal(neighbour({ rects: RECTS, fromWindowId: 3, direction: Direction.up }), 2);
  assert.equal(neighbour({ rects: RECTS, fromWindowId: 3, direction: Direction.left }), 1);
});

test("has nowhere to go past the grid's edge", () => {
  assert.equal(neighbour({ rects: RECTS, fromWindowId: 1, direction: Direction.left }), null);
  assert.equal(neighbour({ rects: RECTS, fromWindowId: 2, direction: Direction.up }), null);
});

test("picks a facing pane when several lie that way", () => {
  assert.ok([2, 3].includes(neighbour({ rects: RECTS, fromWindowId: 1, direction: Direction.right })!));
});
