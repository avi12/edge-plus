import type { AppWindow, Window } from "../winui.ts";
import { OverlappedPresenter, projectAs } from "../winui.ts";
import { scaleOf } from "./native.ts";

// What app code does with an open window: `bind:handle` on <Window> gives it
// one. The native Window and AppWindow are there for what this does not cover.
export class WindowHandle {
  readonly window: Window;
  private isClosed = false;

  constructor(window: Window) {
    this.window = window;
    window.onClosed(() => {
      this.isClosed = true;
    });
  }

  get appWindow(): AppWindow {
    return this.window.appWindow;
  }

  // Device pixels per device-independent pixel (the window's DPI over 96).
  get scale(): number {
    return scaleOf(this.appWindow.id.value);
  }

  // The overlapped presenter (minimize, maximize, restore, the size limits):
  // what a window has until the app gives it another (compact overlay, full
  // screen).
  get presenter(): OverlappedPresenter {
    return projectAs(this.appWindow.presenter, OverlappedPresenter);
  }

  activate(): void {
    this.window.activate();
  }

  // fallow-ignore-next-line unused-class-member
  minimize(): void {
    this.presenter.minimize();
  }

  // fallow-ignore-next-line unused-class-member
  maximize(): void {
    this.presenter.maximize();
  }

  // Out of minimized or maximized to its normal size.
  restore(): void {
    this.presenter.restore();
  }

  // Restored, shown and focused: what launching an app again does.
  // fallow-ignore-next-line unused-class-member
  bringToFront(): void {
    this.restore();
    this.activate();
  }

  // Closing a closed window is a no-op.
  close(): void {
    if (!this.isClosed) {
      this.window.close();
    }
  }
}
