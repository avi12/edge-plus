<script lang="ts">
  import type { Snippet } from "svelte";
  import { TextBlock } from "winui-svelte/components";

  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
</script>

<TextBlock class="section-header">{@render children()}</TextBlock>

<style>
  .section-header {
    margin-bottom: 4px;
    margin-left: 4px;
    -winui-based-on: BodyStrongTextBlockStyle;
  }
</style>
