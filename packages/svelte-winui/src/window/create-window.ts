import type { Mounted } from "../renderer/mount.svelte.ts";
import { mountControl } from "../renderer/mount.svelte.ts";
import { DesktopAcrylicBackdrop, Grid, Window } from "../winui.ts";
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

const chromeDefaults = {
  backdrop: Backdrop.None,
  extendsIntoTitleBar: false,
  title: ""
};

function applyChrome(window: Window, chrome: WindowChrome): void {
  const { backdrop, extendsIntoTitleBar, title } = {
    ...chromeDefaults,
    ...chrome
  };
  applyBackdrop(window, backdrop);
  window.extendsContentIntoTitleBar = extendsIntoTitleBar;
  window.title = title;

  if (chrome.size !== undefined) {
    window.appWindow.resize(chrome.size);
  }
}

// Puts the content in the window; the mounted component, if it is one.
function placeContent<Props extends object>(window: Window, options: WindowOptions<Props>): Mounted<Props> | null {
  if (!isComponentContent(options)) {
    window.content = options.control;
    return null;
  }

  const mounted = mountControl(options.component, options.props);
  // The component's root sits in a grid of the window's, which puts a new root
  // (a hot update's) in the old one's place: a window's own content is not a
  // place a swap can find the old root's parent in.
  const holder = new Grid();
  holder.children.append(mounted.root);
  window.content = holder;
  mounted.onRootChange(root => {
    holder.children.clear();
    holder.children.append(root);
  });
  return mounted;
}

// Opens a window around a component or a control. Call it on the UI thread,
// from `onStart` (application.ts) or later.
export function createWindow<Props extends object>(options: WindowOptions<Props>): AppWindow<Props> {
  const window = new Window();
  applyChrome(window, options);
  const mounted = placeContent(window, options);
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
