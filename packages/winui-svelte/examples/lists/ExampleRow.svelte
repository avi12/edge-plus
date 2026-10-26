<script lang="ts">
  import type { ExampleItem } from "./example-item.ts";
  import { rowStats } from "./row-stats.ts";

  const { row }: { row: ExampleItem } = $props();

  rowStats.created++;
  $effect(() => () => {
    rowStats.destroyed++;
  });
</script>

<stack-panel orientation="Horizontal" padding="12,8,12,8" spacing={12}>
  <text-block text={row.name} />
  <text-block opacity={0.6} text={`#${row.id}`} />
</stack-panel>
