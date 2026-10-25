import type * as motion from "./motion.ts";
import { animate } from "./motion.ts";
import { StackPanel } from "./winui.ts";
import { withControl } from "svelte-winui";

// {@attach animated(motion.reposition)} on a <stack-panel>: motion follows
// Windows' Animation effects switch, so motion.ts holds each panel rather than
// markup setting its transitions.
export function animated(transitions: typeof motion.reposition): (node: unknown) => void {
  return withControl(StackPanel, panel => {
    animate(panel, transitions);
  });
}
