import type { ProbeElement } from "./svelte-tree-probe.ts";
import probeRenderer, { outline } from "./svelte-tree-probe.ts";
import { nextSibling } from "./svelte-tree.ts";
import assert from "node:assert/strict";
import { test } from "node:test";

function panel(): ProbeElement {
  return probeRenderer.createElement("stack-panel");
}

function block(name: string): ProbeElement {
  return probeRenderer.createElement(name);
}

test("insert appends without an anchor and goes before one", () => {
  const parent = panel();
  const anchor = probeRenderer.createComment("");
  probeRenderer.insert(parent, anchor, null);
  probeRenderer.insert(parent, block("a"), anchor);
  probeRenderer.insert(parent, block("b"), anchor);
  probeRenderer.insert(parent, block("z"), null);
  assert.equal(outline(parent), "stack-panel(a() b() # z())");
  assert.equal(parent.native.childChanges, 4, "each insert tells the host once");
});

test("a fragment's children move in order, and the parent hears once", () => {
  const parent = panel();
  const anchor = probeRenderer.createComment("");
  probeRenderer.insert(parent, anchor, null);
  const fragment = probeRenderer.createFragment();
  probeRenderer.insert(fragment, block("a"), null);
  probeRenderer.insert(fragment, probeRenderer.createTextNode("t"), null);
  probeRenderer.insert(fragment, block("b"), null);
  const changesBefore = parent.native.childChanges;

  probeRenderer.insert(parent, fragment, anchor);
  assert.equal(outline(parent), "stack-panel(a() \"t\" b() #)");
  assert.equal(fragment.children.length, 0, "the fragment is emptied");
  assert.ok(parent.children.every(child => child.parent === parent), "every child knows its parent");
  assert.equal(parent.native.childChanges, changesBefore + 1);
});

test("a node already in the tree is moved, not copied", () => {
  const parent = panel();
  const [first, second, third] = [block("a"), block("b"), block("c")];
  for (const child of [first, second, third]) {
    probeRenderer.insert(parent, child, null);
  }

  const changesBefore = parent.native.childChanges;
  probeRenderer.insert(parent, third, first);
  assert.equal(outline(parent), "stack-panel(c() a() b())");
  assert.equal(parent.native.childChanges, changesBefore + 1, "a move within a parent tells it once");

  probeRenderer.insert(parent, first, first);
  assert.equal(outline(parent), "stack-panel(c() a() b())", "a node put before itself stays");

  const other = panel();
  probeRenderer.insert(other, second, null);
  assert.equal(outline(parent), "stack-panel(c() a())", "a move to another parent leaves the first");
  assert.equal(outline(other), "stack-panel(b())");
  assert.equal(second.parent, other);
});

test("next sibling, first and last child, and remove", () => {
  const parent = panel();
  const [first, second] = [block("a"), block("b")];
  probeRenderer.insert(parent, first, null);
  probeRenderer.insert(parent, second, null);
  assert.equal(nextSibling(first), second);
  assert.equal(nextSibling(second), null);
  assert.equal(probeRenderer.getFirstChild(parent), first);
  assert.equal(probeRenderer.getLastChild(parent), second);

  probeRenderer.remove(first);
  assert.equal(outline(parent), "stack-panel(b())");
  assert.equal(first.parent, null);
  assert.equal(nextSibling(first), null, "a detached node has no sibling");
  probeRenderer.remove(first);
  assert.equal(outline(parent), "stack-panel(b())", "removing a detached node does nothing");
});

test("an element's text replaces its children", () => {
  const parent = panel();
  const child = block("a");
  probeRenderer.insert(parent, child, null);
  probeRenderer.setText(parent, "hello");
  assert.equal(outline(parent), "stack-panel(\"hello\")");
  assert.equal(child.parent, null, "the old child is detached");

  const [text] = parent.children;
  assert.ok(text !== undefined);
  const changesBefore = parent.native.childChanges;
  probeRenderer.setText(text, "bye");
  assert.equal(outline(parent), "stack-panel(\"bye\")");
  assert.equal(parent.native.childChanges, changesBefore + 1, "a text change tells its element");
});

// Svelte's hot reload swaps a component's nodes between its anchors: it
// removes the old ones and inserts the new ones before the end anchor.
test("a block between comment anchors is removed and re-inserted", () => {
  const parent = panel();
  const start = probeRenderer.createComment("");
  const end = probeRenderer.createComment("");
  for (const node of [block("before"), start, block("old-a"), block("old-b"), end, block("after")]) {
    probeRenderer.insert(parent, node, null);
  }

  for (let node = nextSibling(start); node !== null && node !== end; node = nextSibling(start)) {
    probeRenderer.remove(node);
  }

  assert.equal(outline(parent), "stack-panel(before() # # after())");
  const fragment = probeRenderer.createFragment();
  probeRenderer.insert(fragment, block("new-a"), null);
  probeRenderer.insert(fragment, block("new-b"), null);
  probeRenderer.insert(parent, fragment, end);
  assert.equal(outline(parent), "stack-panel(before() # new-a() new-b() # after())");
});

test("attributes are kept, and listeners end on removal", () => {
  const element = block("button");
  probeRenderer.setAttribute(element, "margin", 4);
  assert.equal(probeRenderer.getAttribute(element, "margin"), "4");
  assert.ok(probeRenderer.hasAttribute(element, "margin"));
  probeRenderer.removeAttribute(element, "margin");
  assert.equal(probeRenderer.getAttribute(element, "margin"), null);

  function handler() {}
  probeRenderer.addEventListener(element, "click", handler);
  probeRenderer.addEventListener(element, "toggled", handler);
  assert.equal(element.native.subscriptions, 2, "one handler may serve two events");
  probeRenderer.removeEventListener(element, "toggled", handler);
  assert.equal(element.native.subscriptions, 1, "the other event's subscription stays");
  probeRenderer.removeEventListener(element, "click", handler);
  assert.equal(element.native.subscriptions, 0);
});
