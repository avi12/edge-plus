import type { Mounted } from "../renderer/mount.svelte.ts";
import { mountControl } from "../renderer/mount.svelte.ts";
import { DesktopAcrylicBackdrop, Window } from "../winui.ts";
import type { FrameworkElement } from "../winui.ts";
import type { Component } from "svelte";

export const Backdrop = {
  Acrylic: "acrylic",
  None: "none"
} as const;

export type Backdrop = typeof Backdrop[keyof typeof Backdrop];

interface WindowChrome {
  title?: string;
  // In device pixels.
  size?: {
    width: number;
    height: number;
  };
  backdrop?: Backdrop;
  extendsIntoTitleBar?: boolean;
  // Shown without taking focus from whatever the user is doing: for tests and
  // captures.
  isBackground?: boolean;
}

interface ComponentContent<Props extends object> {
  // Mounted, so its props can change and it can be unmounted.
  component: Component<Props>;
  props: Props;
}

interface ControlContent {
  // A control made elsewhere.
  control: FrameworkElement;
}

export type WindowOptions<Props extends object> = WindowChrome & (ComponentContent<Props> | ControlContent);

export interface AppWindow<Props extends object> {
  readonly window: Window;
  // Null when the content was a control.
  readonly mounted: Mounted<Props> | null;
  close(): void;
  onClosed(listener: () => void): void;
}

type ComponentWindowOptions<Props extends object> = WindowChrome & ComponentContent<Props>;

function isComponentContent<Props extends object>(
  options: WindowOptions<Props>
): options is ComponentWindowOptions<Props> {
  return "component" in options;
}

function applyBackdrop(window: Window, backdrop: Backdrop): void {
  if (backdrop === Backdrop.Acrylic) {
    window.systemBackdrop = new DesktopAcrylicBackdrop();
  }
}

// Opens a window around a component or a control. Call it on the UI thread,
// from `onStart` (application.ts) or later.
export function createWindow<Props extends object>(options: WindowOptions<Props>): AppWindow<Props> {
  const window = new Window();
  if (options.title !== undefined) {
    window.title = options.title;
  }

  applyBackdrop(window, options.backdrop ?? Backdrop.None);
  window.extendsContentIntoTitleBar = options.extendsIntoTitleBar ?? false;

  if (options.size !== undefined) {
    window.appWindow.resize(options.size);
  }

  let mounted: Mounted<Props> | null = null;
  if (isComponentContent(options)) {
    mounted = mountControl(options.component, options.props);
    window.content = mounted.root;
  } else {
    window.content = options.control;
  }

  if (options.isBackground === true) {
    window.appWindow.showWithActivation(false);
  } else {
    window.activate();
  }

  return {
    window,
    mounted,
    close() {
      window.close();
    },
    onClosed(listener) {
      window.onClosed(listener);
    }
  };
}
