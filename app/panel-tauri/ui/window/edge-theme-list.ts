import type { ThemeRow } from "../protocol.g.ts";
import EdgeThemeListView from "./edge-theme-list.svelte";
import type { Mounted } from "./mount.svelte.ts";
import { mountControl } from "./mount.svelte.ts";
import * as motion from "./motion.ts";
import { projectAs, StackPanel } from "./winui.ts";

interface Props extends Record<string, unknown> {
  rows: ThemeRow[];
  empty: string | null;
}

// The themes installed in Edge that the user did not add here
// (edge-theme-list.svelte).
export class EdgeThemeList {
  readonly view: StackPanel;
  private readonly list: Mounted<Props>;

  constructor() {
    this.list = mountControl<Props>(EdgeThemeListView, {
      rows: [],
      empty: null
    });
    this.view = projectAs(this.list.root, StackPanel);
    motion.animate(this.view, motion.addDelete);
  }

  update(rows: ThemeRow[], empty: string | null): void {
    this.list.set({
      rows,
      empty
    });
  }
}
