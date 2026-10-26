import type { View } from "../protocol.g.ts";
import PanelPage from "./panel-page.svelte";
import type { PanelActions } from "./panel.svelte.ts";
import { Panel } from "./panel.svelte.ts";
import type { FrameworkElement, Window } from "./winui.ts";
import { mountControl } from "svelte-winui";

// What the page asks of the window it is in.
export interface PanelHost extends PanelActions {
  readonly window: Window;
  refresh(): void;
}

// The window's content (panel-page.svelte). It reads the View the core sends
// from the host's state and never touches settings, Edge or the DLL itself:
// every change is an Action the core runs, answered with a fresh View.
export class PanelView {
  readonly root: FrameworkElement;
  private readonly panel: Panel;

  // `view` is the host's state: the core's later Views change it in place.
  constructor(host: PanelHost, view: View) {
    this.panel = new Panel(host);
    this.root = mountControl(PanelPage, {
      view,
      panel: this.panel
    }).root;
  }

  showActionNotice(message: string): void {
    this.panel.actionNotice = message;
  }
}
