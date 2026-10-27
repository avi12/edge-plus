import type { WindowComponentProps } from "./window-props.ts";
import type { Component } from "svelte";

export type { WindowComponentProps };

declare const Window: Component<WindowComponentProps, Record<string, never>, "handle">;
export default Window;
