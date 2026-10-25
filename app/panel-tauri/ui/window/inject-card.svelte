<script lang="ts">
  import CardFrame from "./card-frame.svelte";
  import type { InjectCardProps } from "./inject-card.d.svelte.ts";

  // The Inject button, as a card under the Inject automatically switch. When it
  // is shown, and whether it can be pressed, is the core's decision.
  const { offer, isPressed, onClicked }: InjectCardProps = $props();

  const canPress = $derived(offer.isEnabled && !isPressed);
</script>

<CardFrame wording={offer.wording}>
  {#snippet end()}
    <button class="AccentButton" is-enabled={canPress} onclick={onClicked}>{offer.button}</button>
  {/snippet}
</CardFrame>
