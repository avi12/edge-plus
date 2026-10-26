<script lang="ts">
  import type { ExampleItem } from "./example-item.ts";
  import { rowStats } from "./row-stats.ts";

  let { row }: { row: ExampleItem } = $props();

  rowStats.created++;
  $effect(() => () => {
    rowStats.destroyed++;
  });
</script>

<stack-panel orientation="Horizontal" spacing={12} padding="12,8,12,8">
  <text-block text={row.name} />
  <text-block text={`#${row.id}`} opacity={0.6} />
</stack-panel>
