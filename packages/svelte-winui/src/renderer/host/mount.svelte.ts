import { flush } from "../../scheduling.ts";
import type { FrameworkElement } from "../../winui.ts";
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
  // Told the new root when the component's root control changes (a hot
  // update, a root inside {#if}). The page has the new one in the old one's
  // place by then when it holds it in a panel's children, a border's child or
  // a content control's content; one that holds it where that cannot be found
  // (a window's content) puts it there itself.
  onRootChange(listener: (root: FrameworkElement) => void): void;
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
  // Plain bookkeeping read by the renderer, not state a component reacts to.
  // eslint-disable-next-line svelte/prefer-svelte-reactivity
  const rootListeners = new Set<(root: FrameworkElement) => void>();
  // A new root (a hot update, or a root inside {#if}) takes the old one's
  // place in the page, which holds the old control by then.
  watchRoot(fragment, control => {
    const isReplaced = control !== null && control !== root;
    if (isReplaced) {
      swapControl(root, control);
      root = control;
      rootListeners.forEach(listener => listener(control));
    }
  });
  return {
    get root() {
      return root;
    },
    onRootChange(listener) {
      rootListeners.add(listener);
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
