import { svelteHooks } from "../vite/loader.ts";
import { FakeBorder, fakeControls, FakePanel, FakeToggle } from "./fake-controls.ts";
import { registerControls, setClassHandler } from "./host.ts";
import assert from "node:assert/strict";
import { registerHooks } from "node:module";
import path from "node:path";
import { test } from "node:test";

// The WinUI renderer's host (host.ts) run on stand-in controls: a real
// component, the real renderer, the control attachment, unmounting and what a
// removed subtree lets go of.

registerControls(fakeControls);
setClassHandler((control, value) => {
  Object.assign(control, { className: value });
});
registerHooks(svelteHooks(path.join(import.meta.dirname, "host.ts")));
const { mountControl } = await import("./mount.svelte.ts");
const { default: Fixture } = await import("./host.fixture.svelte");

interface Props extends Record<string, unknown> {
  isOn: boolean;
  label: string;
  isShown: boolean;
  log: (line: string) => void;
  hosted: object;
}

function mountFixture(overrides: Partial<Props> = {}) {
  const lines: string[] = [];
  const hosted = new FakeBorder();
  const mounted = mountControl<Props>(Fixture, {
    isOn: false,
    label: "On",
    isShown: false,
    log: line => lines.push(line),
    hosted,
    ...overrides
  });
  // Typed as the WinUI control it stands in for.
  const root: unknown = mounted.root;
  assert.ok(root instanceof FakePanel);
  const [toggle, staticToggle] = root.children.items;
  assert.ok(toggle instanceof FakeToggle);
  assert.ok(staticToggle instanceof FakeToggle);
  return {
    mounted,
    root,
    toggle,
    staticToggle,
    lines,
    hosted
  };
}

test("class goes to the app's class handler", () => {
  const { root } = mountFixture();
  assert.equal(root.className, "card");
});

test("the control attachment hands over the typed control and its effects follow the props", () => {
  const { mounted, toggle, lines } = mountFixture({ isOn: true });
  assert.deepEqual(lines, ["attached"]);
  assert.equal(toggle.isOn, true);

  mounted.set({ isOn: false });
  assert.equal(toggle.isOn, false, "an effect inside the attachment re-runs on its own");
  assert.deepEqual(lines, ["attached"], "without attaching again");
});

test("on-content={() => value} is the property OnContent, read when the element is made", () => {
  const { mounted, toggle, staticToggle } = mountFixture({ label: "Active" });
  assert.equal(toggle.onContent, "Active");
  assert.equal(staticToggle.onContent, "Static");

  mounted.set({ label: "Running" });
  assert.equal(toggle.onContent, "Active", "a property that changes is set through the attachment");
});

test("unmount runs the attachment's cleanup and ends the event subscriptions", () => {
  const { mounted, toggle, lines } = mountFixture();
  assert.equal(toggle.subscriptions, 1);

  mounted.unmount();
  assert.deepEqual(lines, ["attached", "cleanup"]);
  assert.equal(toggle.subscriptions, 0);
});

test("a removed subtree ends its subscriptions and releases the control it hosted", () => {
  const { mounted, root, hosted } = mountFixture({ isShown: true });
  const [, , panel] = root.children.items;
  assert.ok(panel instanceof FakePanel);
  const [inner, host] = panel.children.items;
  assert.ok(inner instanceof FakeToggle);
  assert.equal(inner.subscriptions, 1);
  assert.equal(host, hosted, "the hosted control is placed in the subtree");

  mounted.set({ isShown: false });
  assert.equal(root.children.items.length, 2, "the subtree leaves its parent");
  assert.equal(inner.subscriptions, 0);
  assert.equal(panel.children.items.includes(hosted), false, "the hosted control leaves the discarded panel");
});

test("a hosted control can be placed again after its subtree is removed", () => {
  const { mounted, root, hosted } = mountFixture({ isShown: true });
  mounted.set({ isShown: false });
  mounted.set({ isShown: true });
  const [, , panel] = root.children.items;
  assert.ok(panel instanceof FakePanel);
  assert.equal(panel.children.items.includes(hosted), true);
});
