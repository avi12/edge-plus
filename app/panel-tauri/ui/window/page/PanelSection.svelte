<script lang="ts">
  import type { Snippet } from "svelte";
  import { RepositionThemeTransition, StackPanel, TextBlock } from "winui-svelte/components";

  interface Props {
    title: string;
    children: Snippet;
  }

  const { title, children }: Props = $props();
</script>

<StackPanel class="section">
  {#snippet childrenTransitions()}
    <RepositionThemeTransition />
  {/snippet}
  <TextBlock class="section-header">{title}</TextBlock>
  {@render children()}
</StackPanel>

<style>
  .section {
    gap: 4px;
  }

  .section-header {
    margin-bottom: 4px;
    margin-left: 4px;
    -winui-based-on: BodyStrongTextBlockStyle;
  }
</style>
