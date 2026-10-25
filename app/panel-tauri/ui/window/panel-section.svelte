<script lang="ts">
  import type { SectionView, SwitchKey } from "../protocol.g.ts";
  import { animated } from "./animated.ts";
  import * as motion from "./motion.ts";
  import type { FrameworkElement } from "./winui.ts";

  // A section's header over its cards.
  interface Props {
    section: SectionView;
    cardControl: (key: SwitchKey) => FrameworkElement;
    injectCard: FrameworkElement | null;
  }

  const { section, cardControl, injectCard }: Props = $props();
</script>

<stack-panel spacing="4" {@attach animated(motion.reposition)}>
  <!-- The left margin lines the header up with the cards' text, as Windows
       Settings does. -->
  <text-block class="SectionHeader" margin="4,0,0,4">{section.name}</text-block>
  {#each section.cards as card (card.key)}
    {#if card.inject === null}
      <host element={cardControl(card.key)} />
    {:else}
      <!-- Its add/delete transition slides the Inject card in and out. -->
      <stack-panel spacing="4" {@attach animated(motion.addDelete)}>
        <host element={cardControl(card.key)} />
        {#if injectCard !== null}
          <host element={injectCard} />
        {/if}
      </stack-panel>
    {/if}
  {/each}
</stack-panel>
