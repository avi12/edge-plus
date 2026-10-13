// Move between panes: Alt + Shift + an arrow key jumps to the pane on that side of the focused one,
// within the split that is showing.

import { Direction, neighbour } from "./neighbour";
import { shownPaneRects } from "@/features/split-panes/split-panes";
import type { FeatureModule } from "@/lib/feature";
import { Command } from "@/lib/feature";

async function focus(direction: Direction) {
  const window = await browser.windows.getLastFocused({ windowTypes: ["normal"] });
  const rects = window.id === undefined ? null : shownPaneRects(window.id);
  if (!rects) {
    return;
  }
  const target = neighbour({ rects, fromWindowId: window.id!, direction });
  if (target !== null) {
    await browser.windows.update(target, { focused: true });
  }
}

/** Commands only reach a feature the background has switched on, so there is nothing to hold */
export const paneFocus: FeatureModule = {
  async enable() {},
  async disable() {},
  commands: {
    [Command.focusLeft]: () => focus(Direction.left),
    [Command.focusRight]: () => focus(Direction.right),
    [Command.focusUp]: () => focus(Direction.up),
    [Command.focusDown]: () => focus(Direction.down)
  }
};
