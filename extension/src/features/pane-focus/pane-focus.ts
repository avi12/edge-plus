// Move between panes: Alt + Shift + an arrow key jumps to the pane on that side of the focused one,
// in the split tab you are on.

import { Direction } from "./neighbour";
import { focusNeighbour } from "@/features/split-panes/split-panes";
import type { FeatureModule } from "@/lib/feature";
import { Command } from "@/lib/feature";

/** Commands only reach a feature the background has switched on, so there is nothing to hold */
export const paneFocus: FeatureModule = {
  async enable() {},
  async disable() {},
  commands: {
    [Command.focusLeft]: () => focusNeighbour(Direction.left),
    [Command.focusRight]: () => focusNeighbour(Direction.right),
    [Command.focusUp]: () => focusNeighbour(Direction.up),
    [Command.focusDown]: () => focusNeighbour(Direction.down)
  }
};
