<script lang="ts" generics="Item">
  import type { Snippet } from "svelte";
  import type { RowSource } from "./row-source.svelte.ts";

  // One row of a list: the app's `item` snippet, shown for the item the source
  // holds now. Mounted into a container by items-host.svelte.ts.
  let { snippet, source }: { snippet: Snippet<[Item]>; source: RowSource<Item> } = $props();
</script>

{@render snippet(source.current)}
