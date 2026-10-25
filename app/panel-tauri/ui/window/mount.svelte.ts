import renderer, { rootControl, swapControl, watchRoot } from "./svelte-renderer.ts";
import type { FrameworkElement } from "./winui.ts";
import type { Component } from "svelte";
import { flushSync, mount } from "svelte";

// A .svelte component mounted on WinUI: its root control, to put in the page,
// and set(), which changes some of its props; Svelte then updates only what
// reads them. Updates are flushed at once, since WinUI's loop holds the
// thread and Node's microtasks are not a place to wait for them.

export interface Mounted<Props> {
  readonly root: FrameworkElement;
  set(changes: Partial<Props>): void;
}

export function mountControl<Props extends object>(
  component: Component<Props>,
  initial: Props
): Mounted<Props> {
  const props = $state(initial);
  const fragment = renderer.createFragment();
  mount(component, {
    renderer,
    target: fragment,
    props
  });
  flushSync();
  let root = rootControl(fragment);
  // A new root (a hot update, or a root inside {#if}) takes the old one's
  // place in the page, which holds the old control by then.
  watchRoot(fragment, control => {
    const isReplaced = control !== null && control !== root;
    if (isReplaced) {
      swapControl(root, control);
      root = control;
    }
  });
  return {
    get root() {
      return root;
    },
    set(changes) {
      Object.assign(props, changes);
      flushSync();
    }
  };
}
