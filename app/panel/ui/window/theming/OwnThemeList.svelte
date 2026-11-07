<script lang="ts">
  import ThemeRow from "./ThemeRow.svelte";
  import type { ThemeRow as Row } from "#src/protocol.g.ts";
  import {
    AddDeleteThemeTransition,
    ListView,
    ReorderThemeTransition,
    RepositionThemeTransition
  } from "winui-svelte/components";
  import { ListViewSelectionMode, Visibility } from "winui-svelte/enums";

  interface Props {
    rows: Row[];
    onremove: (id: string) => void;
    onreorder: (ids: string[]) => void;
  }

  const { rows, onremove, onreorder }: Props = $props();
</script>

<ListView
  class="own-themes"
  automationHelpText="Drag a theme, or press Alt+Up or Alt+Down, to move it in this list and in the pickers."
  automationName="Your themes"
  isItemClickEnabled={false}
  itemContainerStyle="theme-list-item"
  itemName={row => row.name}
  items={rows}
  key={row => row.id}
  keyboardReorder
  {onreorder}
  reorderable
  selectionMode={ListViewSelectionMode.None}
  visibility={rows.length > 0 ? Visibility.Visible : Visibility.Collapsed}
>
  {#snippet itemContainerTransitions()}
    <AddDeleteThemeTransition />
    <ReorderThemeTransition />
    <RepositionThemeTransition />
  {/snippet}
  {#snippet item(row)}
    <ThemeRow onremove={() => onremove(row.id)} theme={row} />
  {/snippet}
</ListView>

<style>
  .theme-list-item {
    min-height: 36px;
    padding: 0;
  }
</style>
