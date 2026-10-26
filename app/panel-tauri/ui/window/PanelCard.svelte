<script lang="ts">
  import type { CardView } from "../protocol.g.ts";
  import FeatureCard from "./FeatureCard.svelte";
  import InjectCard from "./InjectCard.svelte";
  import * as motion from "./motion.ts";
  import { StackPanel } from "svelte-winui";

  // A feature's card; the one that has an Inject offer carries it under it.
  const { card }: { card: CardView } = $props();
</script>

{#if card.inject === null}
  <FeatureCard {card} />
{:else}
  <!-- Its add/delete transition slides the Inject card in and out. -->
  <StackPanel childrenTransitions={motion.applied(motion.addDelete)} spacing={4}>
    <FeatureCard {card} />
    {#if card.inject.offer !== null}
      <InjectCard offer={card.inject.offer} />
    {/if}
  </StackPanel>
{/if}
