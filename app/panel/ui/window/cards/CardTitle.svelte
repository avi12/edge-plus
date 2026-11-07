<script lang="ts">
  import { useCardLabel } from "./card-label.ts";
  import type { Snippet } from "svelte";
  import { TextBlock } from "winui-svelte/components";

  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
  const label = useCardLabel();
</script>

<TextBlock class="card-title" {@attach label.attach}>{@render children()}</TextBlock>

<style>
  .card-title {
    text-wrap: wrap;
    -winui-based-on: BodyTextBlockStyle;
  }
</style>
