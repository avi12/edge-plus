<script lang="ts">
  import type { Snippet } from "svelte";
  import { TextBlock } from "winui-svelte/components";

  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
</script>

<TextBlock class="card-description">{@render children()}</TextBlock>

<style>
  .card-description {
    color: var(--text-fill-color-secondary);
    text-wrap: wrap;
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
