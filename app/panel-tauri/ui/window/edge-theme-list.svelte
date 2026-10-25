<script lang="ts">
  import type { ThemeRow as Row } from "../protocol.g.ts";
  import ThemeRow from "./theme-row.svelte";
  import { rowKey } from "./theme-row.ts";

  // The themes installed in Edge that the user did not add here: listed, never
  // reordered or removed. `empty` replaces the list (and the user's own) while
  // there are no themes.
  interface Props {
    rows: Row[];
    empty: string | null;
  }

  const { rows, empty }: Props = $props();
</script>

<!-- Keyed by everything a row shows, so an unchanged row is left alone and
     only the rows that came or went play the panel's transition. -->
<stack-panel spacing={4}>
  {#each rows as row (rowKey(row))}
    <ThemeRow theme={row} onRemove={null} />
  {/each}
  {#if empty !== null}
    <text-block class="card-description">{empty}</text-block>
  {/if}
</stack-panel>
