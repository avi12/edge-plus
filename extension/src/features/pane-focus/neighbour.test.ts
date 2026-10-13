import assert from "node:assert/strict";
import { test } from "node:test";
import { Axis, fromTemplate, layout } from "../split-panes/layout.ts";
import { Direction, neighbour } from "./neighbour.ts";

// One big pane on the left (a), two stacked on the right (b over c)
const RECTS = layout({
  root: fromTemplate({
    template: { axis: Axis.row, children: [0, { axis: Axis.column, children: [1, 2] }] },
    panes: ["a", "b", "c"].map(id => ({ id, url: "" }))
  })
});

test("moves to the pane on that side", () => {
  assert.equal(neighbour({ rects: RECTS, fromPaneId: "b", direction: Direction.down }), "c");
  assert.equal(neighbour({ rects: RECTS, fromPaneId: "c", direction: Direction.up }), "b");
  assert.equal(neighbour({ rects: RECTS, fromPaneId: "c", direction: Direction.left }), "a");
});

test("has nowhere to go past the grid's edge", () => {
  assert.equal(neighbour({ rects: RECTS, fromPaneId: "a", direction: Direction.left }), null);
  assert.equal(neighbour({ rects: RECTS, fromPaneId: "b", direction: Direction.up }), null);
});

test("picks a facing pane when several lie that way", () => {
  assert.ok(["b", "c"].includes(neighbour({ rects: RECTS, fromPaneId: "a", direction: Direction.right })!));
});
