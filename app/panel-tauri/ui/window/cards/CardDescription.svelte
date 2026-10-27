<script lang="ts">
  import type { Snippet } from "svelte";
  import { TextBlock } from "winui-svelte";

  // What a card's switch or button does, under its title, written as the
  // element's children.
  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
</script>

<TextBlock class="card-description">{@render children()}</TextBlock>
