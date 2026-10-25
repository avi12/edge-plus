import { flush } from "../scheduling.ts";
import type { FrameworkElement } from "../winui.ts";
import renderer, {
  releaseFragment,
  rootControl,
  swapControl,
  unwatchRoot,
  watchRoot
} from "./host.ts";
import type { Component } from "svelte";
import { mount, unmount } from "svelte";

// A .svelte component mounted on WinUI: its root control, to put in the page,
// and set(), which changes some of its props; Svelte then updates only what
// reads them. Updates are flushed at once, since WinUI's loop holds the
// thread and Node's microtasks are not a place to wait for them.

export interface Mounted<Props> {
  readonly root: FrameworkElement;
  set(changes: Partial<Props>): void;
  // Ends the component's effects (attachment cleanups included) and lets go
  // of the controls it made, except the ones the app placed in it. The root
  // control is left where the page has it, for the page to drop.
  unmount(): void;
}

export function mountControl<Props extends object>(
  component: Component<Props>,
  initial: Props
): Mounted<Props> {
  const props = $state(initial);
  const fragment = renderer.createFragment();
  const instance = mount(component, {
    renderer,
    target: fragment,
    props
  });
  flush();
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
      flush();
    },
    unmount() {
      // Before Svelte removes the nodes, so the page is not told of a root
      // that is going.
      unwatchRoot(fragment);
      void unmount(instance);
      releaseFragment(fragment);
    }
  };
}
