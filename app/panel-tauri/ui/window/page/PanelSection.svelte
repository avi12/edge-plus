<script lang="ts">
  import type { Snippet } from "svelte";
  import { RepositionThemeTransition, StackPanel } from "winui-svelte/components";

  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
</script>

<StackPanel class="section">
  {#snippet childrenTransitions()}
    <RepositionThemeTransition />
  {/snippet}
  {@render children()}
</StackPanel>

<style>
  .section {
    gap: 4px;
  }
</style>
