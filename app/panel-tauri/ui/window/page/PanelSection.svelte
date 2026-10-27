<script lang="ts">
  import * as motion from "../style/motion.ts";
  import type { Snippet } from "svelte";
  import { StackPanel, TextBlock } from "winui-svelte";

  // A section's header over its cards.
  const { name, children }: {
    name: string;
    children: Snippet;
  } = $props();
</script>

<StackPanel childrenTransitions={motion.applied(motion.reposition)} spacing={4}>
  <!-- The left margin lines the header up with the cards' text, as Windows
       Settings does. -->
  <TextBlock class="section-header" margin="4,0,0,4" text={name} />
  {@render children()}
</StackPanel>

<style>
  :global(.section-header) {
    -winui-based-on: BodyStrongTextBlockStyle;
  }
</style>
