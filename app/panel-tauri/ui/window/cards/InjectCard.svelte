<script lang="ts">
  import AccentButton from "./AccentButton.svelte";
  import CardDescription from "./CardDescription.svelte";
  import CardFrame from "./CardFrame.svelte";
  import CardTitle from "./CardTitle.svelte";
  import { GLYPHS } from "./glyphs.ts";
  import type { InjectOffer } from "#src/protocol.g.ts";
  import { runAndReport, usePanel } from "#src/window/host/panel.svelte.ts";

  interface Props {
    offer: Exclude<InjectOffer, "hidden">;
  }

  const { offer }: Props = $props();
  const panel = usePanel();

  const isReady = $derived(offer === "ready");

  let isPressed = $state(false);
  const canPress = $derived(isReady && !isPressed);

  async function inject() {
    isPressed = true;
    await runAndReport({
      panel,
      action: {
        kind: "inject"
      }
    });
    isPressed = false;
  }
</script>

<CardFrame glyph={isReady ? GLYPHS.injectReady : GLYPHS.injected}>
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
    <AccentButton automationId="inject" isEnabled={canPress} onclick={inject}>Inject into Edge</AccentButton>
  {/snippet}
</CardFrame>
