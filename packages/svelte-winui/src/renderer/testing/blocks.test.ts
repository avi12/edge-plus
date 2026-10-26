import { svelteHooks } from "../../vite/loader.ts";
import { NodeType } from "../tree/tree.ts";
import type { ProbeElement } from "./tree-probe.ts";
import probeRenderer, { outline } from "./tree-probe.ts";
import assert from "node:assert/strict";
import { registerHooks } from "node:module";
import path from "node:path";
import { test } from "node:test";

// A real component through the tree: {#if} and keyed {#each} anchor on
// comments, insert before them, and a reorder moves the same nodes.

registerHooks(svelteHooks(path.join(import.meta.dirname, "tree-probe.ts")));
const { flushSync, mount } = await import("svelte");
const { default: Blocks } = await import("./Blocks.fixture.svelte");

function elementsOf(panel: ProbeElement): ProbeElement[] {
  return panel.children.flatMap(child => child.type === NodeType.Element ? [child] : []);
}

// The panel's text blocks, by their text: what the window would show.
function shown(panel: ProbeElement): string {
  return elementsOf(panel).map(element => outline(element).replaceAll(/^text-block\("|"\)$/gu, "")).join(",");
}

test("{#if} and keyed {#each} render, update and reorder through the tree", () => {
  const target = probeRenderer.createFragment();
  const blocks = mount(Blocks, {
    target,
    renderer: probeRenderer
  });
  flushSync();
  const [panel] = target.children;
  assert.ok(panel?.type === NodeType.Element);
  assert.equal(shown(panel), "header,a,b,c,footer");

  const [, itemA, itemB, itemC] = elementsOf(panel);
  blocks.showItems(["c", "a", "b"]);
  flushSync();
  assert.equal(shown(panel), "header,c,a,b,footer", "a keyed reorder");
  const [, movedC, movedA, movedB] = elementsOf(panel);
  assert.ok(movedC === itemC && movedA === itemA && movedB === itemB, "moves the same nodes");

  blocks.showItems(["a", "d"]);
  flushSync();
  assert.equal(shown(panel), "header,a,d,footer", "removes and adds by key");

  blocks.showHeader(false);
  flushSync();
  assert.equal(shown(panel), "a,d,footer", "an {#if} turned off leaves");

  blocks.showHeader(true);
  flushSync();
  assert.equal(shown(panel), "header,a,d,footer", "and comes back before its anchor");

  blocks.showItems([]);
  flushSync();
  assert.equal(shown(panel), "header,footer", "an emptied {#each}");
});
