import type { WindowHandle } from "../../window/window-handle.ts";
import type { WindowProps } from "../../window/window-host.svelte.ts";
import type { Component, Snippet } from "svelte";

export interface WindowComponentProps extends WindowProps {
  // The open window, for code that closes it, restores it or brings it to the
  // front: `bind:handle`.
  handle?: WindowHandle;
  // The custom title bar: the window's drag region (clicks on controls inside
  // it still reach them). Shown above the content, which then runs under it.
  titleBar?: Snippet;
  children?: Snippet;
}

declare const Window: Component<WindowComponentProps, Record<string, never>, "handle">;
export default Window;
