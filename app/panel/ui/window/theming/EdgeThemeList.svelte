<script lang="ts">
  import ThemeRow from "./ThemeRow.svelte";
  import type { ThemeRow as Row } from "#src/protocol.g.ts";
  import CardDescription from "#src/window/cards/CardDescription.svelte";
  import { AddDeleteThemeTransition, StackPanel } from "winui-svelte/components";

  interface Props {
    rows: Row[];
    hasThemes: boolean;
  }

  const { rows, hasThemes }: Props = $props();

  const ROW_KEY_SEPARATOR = "\n";

  function rowKey(row: Row) {
    return [row.id, row.name, row.badges.join(",")].join(ROW_KEY_SEPARATOR);
  }
</script>

<StackPanel class="edge-themes">
  {#snippet childrenTransitions()}
    <AddDeleteThemeTransition />
  {/snippet}
  {#each rows as row (rowKey(row))}
    <ThemeRow onremove={null} theme={row} />
  {/each}
  {#if !hasThemes}
    <CardDescription>No themes yet - paste a store link or add a local theme (.crx, .zip or folder).</CardDescription>
  {/if}
</StackPanel>

<style>
  .edge-themes {
    gap: 4px;
  }
</style>
