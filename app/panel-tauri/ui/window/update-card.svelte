<script lang="ts">
  import CardFrame from "./card-frame.svelte";
  import type { UpdateCardProps } from "./update-card.d.svelte.ts";

  // A release that is downloaded and checked, with the button that restarts
  // Edge+ into it. Hidden until one is ready.
  const { update, isPressed, onClicked }: UpdateCardProps = $props();

  const hiddenWording = {
    title: "",
    description: "",
    glyph: 0
  };

  const label = $derived(isPressed ? update?.restarting : update?.button);
</script>

<CardFrame wording={update?.wording ?? hiddenWording} isShown={update !== null}>
  {#snippet end()}
    <button class="accent-button" is-enabled={!isPressed} onclick={onClicked}>{label ?? ""}</button>
  {/snippet}
</CardFrame>
