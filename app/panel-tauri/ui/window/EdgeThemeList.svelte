<script lang="ts">
  import type { ThemeRow as Row } from "../protocol.g.ts";
  import * as motion from "./motion.ts";
  import ThemeRow from "./ThemeRow.svelte";
  import { StackPanel, TextBlock } from "svelte-winui";

  // The themes installed in Edge that the user did not add here: listed, never
  // reordered or removed. `empty` replaces the list (and the user's own) while
  // there are no themes.
  interface Props {
    rows: Row[];
    empty: string | null;
  }

  const { rows, empty }: Props = $props();

  const rowKeySeparator = "\n";

  // Everything a row shows, so an unchanged row is left alone (and does not
  // re-animate).
  function rowKey(row: Row): string {
    return [row.id, row.name, row.badges.join(",")].join(rowKeySeparator);
  }
</script>

<!-- Keyed by everything a row shows, so an unchanged row is left alone and
     only the rows that came or went play the panel's transition. -->
<StackPanel spacing={4} childrenTransitions={motion.applied(motion.addDelete)}>
  {#each rows as row (rowKey(row))}
    <ThemeRow theme={row} onRemove={null} />
  {/each}
  {#if empty !== null}
    <TextBlock class="card-description" text={empty} />
  {/if}
</StackPanel>
