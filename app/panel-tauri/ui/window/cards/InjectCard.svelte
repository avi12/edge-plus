<script lang="ts">
  import type { InjectOffer } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import CardDescription from "./CardDescription.svelte";
  import CardFrame from "./CardFrame.svelte";
  import CardTitle from "./CardTitle.svelte";
  import { Button } from "winui-svelte";

  // The Inject button, as a card under the Inject automatically switch. When it
  // is shown, and whether it can be pressed, is the core's decision: ready, or
  // injected and so disabled. One card either way, so the change is in place.
  interface Props {
    offer: Exclude<InjectOffer, "hidden">;
  }

  const { offer }: Props = $props();
  const panel = usePanel();

  const isReady = $derived(offer === "ready");

  // Pressed and not yet answered: the button stays disabled.
  let isPressed = $state(false);
  const canPress = $derived(isReady && !isPressed);

  async function inject(): Promise<void> {
    // Disabled at once, so a second click cannot queue a second injection.
    isPressed = true;
    await panel.runAndReport({ kind: "inject" });
    // A failed injection renders no new view; the button the click disabled
    // comes back as the offer last shown.
    isPressed = false;
  }
</script>

<CardFrame glyph={isReady ? 0xE768 : 0xE73E}>
  <CardTitle>
    {#if isReady}
      Edge is open without Edge+
    {:else}
      Edge+ is in Edge
    {/if}
  </CardTitle>
  <CardDescription>
    {#if isReady}
      Inject automatically is off, so this Edge started without Edge+. Inject it to turn your switches on.
    {:else}
      Injected. When Edge next starts, Edge+ offers this button again.
    {/if}
  </CardDescription>
  {#snippet end()}
    <Button class="accent-button" isEnabled={canPress} onClick={() => void inject()}>Inject into Edge</Button>
  {/snippet}
</CardFrame>
