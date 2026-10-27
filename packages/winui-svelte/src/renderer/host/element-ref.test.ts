import { svelteHooks } from "../../vite/loader.ts";
import { fakeControls, FakePanel, FakeTextBlock, FakeToggle } from "../testing/fake-controls.ts";
import { registerControls } from "./host.ts";
import assert from "node:assert/strict";
import { registerHooks } from "node:module";
import path from "node:path";
import { test } from "node:test";

// elementRef through a component: the attachment written on the component
// reaches its element's control, and the ref follows the element's life.

registerControls(fakeControls);
registerHooks(svelteHooks(path.join(import.meta.dirname, "host.ts")));
const { mountControl } = await import("./mount.svelte.ts");
const { default: Labeled } = await import("../testing/Labeled.fixture.svelte");

interface Props {
  isLabelShown: boolean;
}

test("a ref on a component holds its control while it is shown", () => {
  const mounted = mountControl<Props>(Labeled, { isLabelShown: true });
  const root: object = mounted.root;
  assert.ok(root instanceof FakePanel);
  const [label, toggle] = root.children.items;
  assert.ok(label instanceof FakeTextBlock);
  assert.ok(toggle instanceof FakeToggle);
  assert.equal(label.text, "Start with Windows");
  assert.equal(toggle.labeledBy, label);

  mounted.set({ isLabelShown: false });
  assert.equal(toggle.labeledBy, null);

  mounted.set({ isLabelShown: true });
  const [shownAgain] = root.children.items;
  assert.ok(shownAgain instanceof FakeTextBlock);
  assert.equal(toggle.labeledBy, shownAgain);
  mounted.unmount();
});
