<script lang="ts">
  import type { ThemeRow as Row } from "../../protocol.g.ts";
  import * as motion from "../style/motion.ts";
  import ThemeRow from "./ThemeRow.svelte";
  import { StackPanel, TextBlock } from "winui-svelte";

  // The themes installed in Edge that the user did not add here: listed, never
  // reordered or removed. While there are no themes at all, a hint stands in
  // for this list and the user's own.
  interface Props {
    rows: Row[];
    hasThemes: boolean;
  }

  const { rows, hasThemes }: Props = $props();

  const rowKeySeparator = "\n";

  // Everything a row shows, so an unchanged row is left alone (and does not
  // re-animate).
  function rowKey(row: Row): string {
    return [row.id, row.name, row.badges.join(",")].join(rowKeySeparator);
  }
</script>

<!-- Keyed by everything a row shows, so an unchanged row is left alone and
     only the rows that came or went play the panel's transition. -->
<StackPanel class="edge-themes" childrenTransitions={motion.applied(motion.addDelete)}>
  {#each rows as row (rowKey(row))}
    <ThemeRow onRemove={null} theme={row} />
  {/each}
  {#if !hasThemes}
    <TextBlock class="card-description" text="No themes yet — paste a store link or add a local theme (.crx, .zip or folder)." />
  {/if}
</StackPanel>

<!-- Global: a class on a component is not an element's, so Svelte does not
     scope it. -->
<style>
  :global(.edge-themes) {
    gap: 4px;
  }
</style>
