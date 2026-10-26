<script lang="ts">
  import type { InjectView } from "../protocol.g.ts";
  import CardFrame from "./CardFrame.svelte";
  import { usePanel } from "./panel.svelte.ts";
  import { Button } from "svelte-winui";

  // The Inject button, as a card under the Inject automatically switch. When it
  // is shown, and whether it can be pressed, is the core's decision.
  const { offer }: { offer: InjectView } = $props();
  const panel = usePanel();

  // Pressed and not yet answered: the button stays disabled.
  let isPressed = $state(false);
  const canPress = $derived(offer.isEnabled && !isPressed);

  async function inject(): Promise<void> {
    // Disabled at once, so a second click cannot queue a second injection.
    isPressed = true;
    await panel.runAndReport({ kind: "inject" });
    // A failed injection renders no new view; the button the click disabled
    // comes back as the offer last shown.
    isPressed = false;
  }
</script>

<CardFrame wording={offer.wording}>
  {#snippet end()}
    <Button class="accent-button" isEnabled={canPress} onClick={() => void inject()}>{offer.button}</Button>
  {/snippet}
</CardFrame>
