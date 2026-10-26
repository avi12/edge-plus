<script lang="ts">
  import type { ThemeRow as Row, ThemingLabels } from "../protocol.g.ts";
  import * as motion from "./motion.ts";
  import { themeListItemStyle } from "./styles.ts";
  import ThemeRow from "./ThemeRow.svelte";
  import { ListView } from "svelte-winui";

  // The user's own themes, in the order of the pickers, reordered by dragging
  // or with Alt+Up / Alt+Down. No selection: with it on, each dragged row kept
  // the accent bar, so two drags looked like two selected themes.
  interface Props {
    rows: Row[];
    labels: ThemingLabels;
    onRemove: (id: string) => void;
    // The ids in the order the user left them in.
    onReorder: (ids: string[]) => void;
  }

  const {
    rows,
    labels,
    onRemove,
    onReorder
  }: Props = $props();
</script>

<ListView
  automationHelpText={labels.reorderHelp}
  automationName={labels.themeListName}
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
  visibility={rows.length > 0 ? "Visible" : "Collapsed"}
>
  {#snippet item(row)}
    <ThemeRow onRemove={() => onRemove(row.id)} theme={row} />
  {/snippet}
</ListView>
