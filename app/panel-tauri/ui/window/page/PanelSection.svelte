<script lang="ts">
  import type { SectionView } from "../../protocol.g.ts";
  import * as motion from "../style/motion.ts";
  import PanelCard from "./PanelCard.svelte";
  import { StackPanel, TextBlock } from "svelte-winui";

  // A section's header over its cards.
  const { section }: { section: SectionView } = $props();
</script>

<StackPanel childrenTransitions={motion.applied(motion.reposition)} spacing={4}>
  <!-- The left margin lines the header up with the cards' text, as Windows
       Settings does. -->
  <TextBlock class="section-header" margin="4,0,0,4" text={section.name} />
  {#each section.cards as card (card.key)}
    <PanelCard {card} />
  {/each}
</StackPanel>

<style>
  :global(.section-header) {
    -winui-based-on: BodyStrongTextBlockStyle;
  }
</style>
