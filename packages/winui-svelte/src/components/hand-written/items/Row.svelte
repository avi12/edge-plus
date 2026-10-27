<script generics="Item" lang="ts">
  import type { RowProps } from "./row-source.svelte.ts";

  // One row of a list: the app's `item` snippet, shown for the item the source
  // holds now. Mounted into a container by items-host.svelte.ts.
  const { snippet, source }: RowProps<Item> = $props();
</script>

{@render snippet(source.current)}
