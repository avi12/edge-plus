<script lang="ts">
  import type { ThemeRow as Row, ThemingLabels } from "../protocol.g.ts";
  import * as motion from "./motion.ts";
  import { themeListItemStyle } from "./styles.ts";
  import ThemeRow from "./theme-row.svelte";
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
  items={rows}
  key={row => row.id}
  itemName={row => row.name}
  reorderable
  keyboardReorder
  {onReorder}
  selectionMode="None"
  isItemClickEnabled={false}
  itemContainerStyle={themeListItemStyle()}
  automationName={labels.themeListName}
  automationHelpText={labels.reorderHelp}
  visibility={rows.length > 0 ? "Visible" : "Collapsed"}
  onControl={view => motion.animateItems(view, motion.listItems)}
>
  {#snippet item(row)}
    <ThemeRow theme={row} onRemove={() => onRemove(row.id)} />
  {/snippet}
</ListView>
