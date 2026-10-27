<script lang="ts">
  import * as motion from "../style/motion.ts";
  import type { Snippet } from "svelte";
  import { StackPanel, TextBlock } from "winui-svelte";

  // A section's header over its cards.
  interface Props {
    name: string;
    children: Snippet;
  }

  const { name, children }: Props = $props();
</script>

<StackPanel class="section" childrenTransitions={motion.applied(motion.reposition)}>
  <TextBlock class="section-header">{name}</TextBlock>
  {@render children()}
</StackPanel>

<style>
  :global(.section) {
    gap: 4px;
  }

  /* The left margin lines the header up with the cards' text, as Windows
     Settings does. */
  :global(.section-header) {
    margin-bottom: 4px;
    margin-left: 4px;
    -winui-based-on: BodyStrongTextBlockStyle;
  }
</style>
