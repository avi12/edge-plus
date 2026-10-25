import type { ThemeRow } from "../protocol.g.ts";
import EdgeThemeListView from "./edge-theme-list.svelte";
import * as motion from "./motion.ts";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import type { FrameworkElement } from "./winui.ts";
import { projectAs, StackPanel } from "./winui.ts";

interface Props extends Record<string, unknown> {
  rows: ThemeRow[];
  empty: string | null;
}

// The themes installed in Edge that the user did not add here
// (edge-theme-list.svelte).
export class EdgeThemeList {
  readonly view: FrameworkElement;
  private readonly list: Mounted<Props>;

  constructor() {
    this.list = mountControl<Props>(EdgeThemeListView, {
      rows: [],
      empty: null
    });
    this.view = this.list.root;
    motion.animate(projectAs(this.list.root, StackPanel), motion.addDelete);
  }

  update(rows: ThemeRow[], empty: string | null): void {
    this.list.set({
      rows,
      empty
    });
  }
}
