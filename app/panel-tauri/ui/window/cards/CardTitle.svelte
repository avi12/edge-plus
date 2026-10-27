<script lang="ts">
  import { useCardLabel } from "./card-label.ts";
  import type { Snippet } from "svelte";
  import { TextBlock } from "winui-svelte";

  // A card's title, its text written as the element's children. The control
  // at the card's end is named by it.
  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
  const label = useCardLabel();
</script>

<TextBlock class="card-title" {@attach label.attach}>{@render children()}</TextBlock>
