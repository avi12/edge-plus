<script lang="ts">
  import type { ThemeRow as Row } from "../../protocol.g.ts";
  import * as motion from "../style/motion.ts";
  import { themeListItemStyle } from "../style/styles.ts";
  import ThemeRow from "./ThemeRow.svelte";
  import { ListView } from "winui-svelte";

  // The user's own themes, in the order of the pickers, reordered by dragging
  // or with Alt+Up / Alt+Down. No selection: with it on, each dragged row kept
  // the accent bar, so two drags looked like two selected themes.
  interface Props {
    rows: Row[];
    onRemove: (id: string) => void;
    // The ids in the order the user left them in.
    onReorder: (ids: string[]) => void;
  }

  const {
    rows,
    onRemove,
    onReorder
  }: Props = $props();
</script>

<ListView
  class={rows.length > 0 ? "" : "collapsed"}
  automationHelpText="Drag a theme, or press Alt+Up or Alt+Down, to move it in this list and in the pickers."
  automationName="Your themes"
  isItemClickEnabled={false}
  itemContainerStyle={themeListItemStyle()}
  itemName={row => row.name}
  items={rows}
  key={row => row.id}
  keyboardReorder
  onControl={view => motion.animateItems(view, motion.listItems)}
  {onReorder}
  reorderable
  selectionMode="None"
>
  {#snippet item(row)}
    <ThemeRow onRemove={() => onRemove(row.id)} theme={row} />
  {/snippet}
</ListView>
