import type { FrameworkElement } from "../winui.ts";
import { DisplayArea, DisplayAreaFallback, Window, WindowActivationState } from "../winui.ts";
import type { Backdrop } from "./backdrop.ts";
import { applyBackdrop, Backdrop as BackdropKind } from "./backdrop.ts";
import { joinAppUserModel } from "./native.ts";
import { WindowHandle } from "./window-handle.ts";

// What <Window> (components/hand-written/Window.svelte) does: a native window
// around the component's content, kept in line with its props.

// In device pixels, as AppWindow has them.
export interface WindowBounds {
  x: number;
  y: number;
  width: number;
  height: number;
}

// Where the window opens, in device pixels; a size left out is the width or
// height prop's.
export interface WindowPlacement {
  x: number;
  y: number;
  width?: number;
  height?: number;
}

export interface WindowProps {
  title?: string;
  // The material behind the content; none (the default) is the theme's color.
  backdrop?: Backdrop;
  // The content runs up into the title bar. On by default with a `titleBar`.
  extendsIntoTitleBar?: boolean;
  // The path of an .ico file, for the title bar and the taskbar.
  icon?: string;
  // Groups the process's windows on the taskbar (see joinAppUserModel). Read
  // once, before the window opens.
  appUserModelId?: string;
  // The size in device-independent pixels (the scale of the window's display
  // applied); the window opens at it and follows it when it changes. Without
  // them the window has the system's default size.
  width?: number;
  height?: number;
  // The least and most the user can resize to, in device-independent pixels.
  minWidth?: number;
  minHeight?: number;
  maxWidth?: number;
  maxHeight?: number;
  isResizable?: boolean;
  isMaximizable?: boolean;
  isMinimizable?: boolean;
  // Where it opens; without it the window opens centered on its display
  // (isCentered={false} leaves it where the system puts it).
  placement?: WindowPlacement | null;
  isCentered?: boolean;
  // false opens the window without taking focus from whatever the user is
  // doing (tests, captures, background starts). Read once.
  activate?: boolean;
  // The window became the active one (true) or lost that to another (false).
  onActiveChanged?: (isActive: boolean) => void;
  onClosed?: () => void;
  // Moved or resized, by the user or by code; the bounds in device pixels.
  onBoundsChanged?: (bounds: WindowBounds) => void;
}

function round(value: number, scale: number): number {
  return Math.round(value * scale);
}

class WindowHost {
  readonly handle: WindowHandle;
  private readonly props: WindowProps;
  private readonly titleBar: () => FrameworkElement | null;
  private readonly unsubscribe: Array<() => void> = [];
  private appliedBackdrop: Backdrop | null = null;
  private appliedSize: {
    width?: number;
    height?: number;
  };

  constructor(content: FrameworkElement, props: WindowProps, titleBar: () => FrameworkElement | null) {
    this.props = props;
    this.titleBar = titleBar;
    this.appliedSize = {
      width: props.width,
      height: props.height
    };

    if (props.appUserModelId !== undefined) {
      joinAppUserModel(props.appUserModelId);
    }

    const window = new Window();
    this.handle = new WindowHandle(window);
    window.content = content;
    this.keepInLine(() => this.syncText());
    this.keepInLine(() => this.syncBackdrop());
    this.keepInLine(() => this.syncTitleBar());
    this.keepInLine(() => this.syncLimits());
    this.placeFirst();
    $effect(() => this.syncSize());
    this.listen();
    this.show();
  }

  dispose(): void {
    for (const unsubscribe of this.unsubscribe.splice(0)) {
      unsubscribe();
    }

    this.handle.close();
  }

  // Applied now, so the window opens as it is meant to, and again whenever
  // what `sync` reads changes.
  private keepInLine(sync: () => void): void {
    sync();
    $effect(sync);
  }

  private syncText(): void {
    const { window } = this.handle;
    window.title = this.props.title ?? "";
    const icon = this.props.icon;
    if (icon !== undefined) {
      window.appWindow.setIcon(icon);
    }
  }

  private syncBackdrop(): void {
    const backdrop = this.props.backdrop ?? BackdropKind.None;
    const isApplied = backdrop === this.appliedBackdrop;
    if (!isApplied) {
      applyBackdrop(this.handle.window, backdrop);
      this.appliedBackdrop = backdrop;
    }
  }

  private syncTitleBar(): void {
    const bar = this.titleBar();
    const { window } = this.handle;
    const isExtended = this.props.extendsIntoTitleBar ?? bar !== null;
    window.extendsContentIntoTitleBar = isExtended;
    const isBarSet = isExtended && bar !== null;
    if (isBarSet) {
      window.setTitleBar(bar);
    }
  }

  private syncLimits(): void {
    const { props } = this;
    const presenter = this.handle.presenter;
    const scale = this.handle.scale;
    if (props.isResizable !== undefined) {
      presenter.isResizable = props.isResizable;
    }

    if (props.isMaximizable !== undefined) {
      presenter.isMaximizable = props.isMaximizable;
    }

    if (props.isMinimizable !== undefined) {
      presenter.isMinimizable = props.isMinimizable;
    }

    if (props.minWidth !== undefined) {
      presenter.preferredMinimumWidth = round(props.minWidth, scale);
    }

    if (props.minHeight !== undefined) {
      presenter.preferredMinimumHeight = round(props.minHeight, scale);
    }

    if (props.maxWidth !== undefined) {
      presenter.preferredMaximumWidth = round(props.maxWidth, scale);
    }

    if (props.maxHeight !== undefined) {
      presenter.preferredMaximumHeight = round(props.maxHeight, scale);
    }
  }

  // The size and place it opens at: where it was (a dev swap puts the new
  // window where the old one was), else centered.
  private placeFirst(): void {
    const { props } = this;
    const { appWindow } = this.handle.window;
    const scale = this.handle.scale;
    const size = {
      width: props.width === undefined ? appWindow.size.width : round(props.width, scale),
      height: props.height === undefined ? appWindow.size.height : round(props.height, scale)
    };
    const placement = props.placement ?? null;
    if (placement !== null) {
      appWindow.moveAndResize({
        x: placement.x,
        y: placement.y,
        width: placement.width ?? size.width,
        height: placement.height ?? size.height
      });
      return;
    }

    appWindow.resize(size);
    const isCentered = props.isCentered ?? true;
    if (isCentered) {
      this.center();
    }
  }

  private center(): void {
    const { appWindow } = this.handle.window;
    const display = DisplayArea.getFromWindowId(appWindow.id, DisplayAreaFallback.Nearest);
    const area = display.workArea;
    appWindow.move({
      x: area.x + Math.round((area.width - appWindow.size.width) / 2),
      y: area.y + Math.round((area.height - appWindow.size.height) / 2)
    });
  }

  // A new width or height resizes the window; the user's own resizing is not
  // undone, since the props did not change.
  private syncSize(): void {
    const { width, height } = this.props;
    const isChanged = width !== this.appliedSize.width || height !== this.appliedSize.height;
    if (!isChanged) {
      return;
    }

    this.appliedSize = {
      width,
      height
    };
    const { appWindow } = this.handle.window;
    const scale = this.handle.scale;
    appWindow.resize({
      width: width === undefined ? appWindow.size.width : round(width, scale),
      height: height === undefined ? appWindow.size.height : round(height, scale)
    });
  }

  private listen(): void {
    const { window } = this.handle;
    this.unsubscribe.push(
      window.onActivated((_sender, event) => {
        this.props.onActiveChanged?.(event.windowActivationState !== WindowActivationState.Deactivated);
      }),
      window.onClosed(() => this.props.onClosed?.()),
      window.appWindow.onChanged((_sender, event) => {
        const isMoved = event.didPositionChange || event.didSizeChange;
        if (isMoved) {
          const { position, size } = window.appWindow;
          this.props.onBoundsChanged?.({
            x: position.x,
            y: position.y,
            width: size.width,
            height: size.height
          });
        }
      })
    );
  }

  private show(): void {
    const { window } = this.handle;
    if (this.props.activate === false) {
      window.appWindow.showWithActivation(false);
    } else {
      window.activate();
    }
  }
}

export interface AttachedWindow {
  handle: WindowHandle;
  dispose(): void;
}

// Opens the window around `content`; `titleBar` is read whenever it is needed
// (it is state in the component, the element of its titleBar snippet).
export function attachWindow(
  content: FrameworkElement,
  props: WindowProps,
  titleBar: () => FrameworkElement | null
): AttachedWindow {
  const host = new WindowHost(content, props, titleBar);
  return {
    handle: host.handle,
    dispose: () => host.dispose()
  };
}
