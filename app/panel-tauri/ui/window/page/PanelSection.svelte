<script lang="ts">
  import * as motion from "../style/motion.ts";
  import type { Snippet } from "svelte";
  import { StackPanel } from "winui-svelte";

  // A section: its SectionHeader, then its cards.
  interface Props {
    children: Snippet;
  }

  const { children }: Props = $props();
</script>

<StackPanel class="section" childrenTransitions={motion.applied(motion.reposition)}>
  {@render children()}
</StackPanel>

<style>
  :global(.section) {
    gap: 4px;
  }
</style>
