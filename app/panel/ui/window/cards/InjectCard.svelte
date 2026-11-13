<script lang="ts">
  import "#src/window/style/accent-button.css";
  import CardDescription from "./CardDescription.svelte";
  import CardFrame from "./CardFrame.svelte";
  import CardTitle from "./CardTitle.svelte";
  import type { InjectOffer } from "#src/protocol.g.ts";
  import { runAndReport, usePanel } from "#src/window/host/panel.svelte.ts";
  import { Button } from "winui-svelte/components";

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

<CardFrame glyph={isReady ? "Play" : "CheckMark"}>
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
    <Button class="accent-button" automationId="inject" isEnabled={canPress} onclick={inject}>Inject into Edge</Button>
  {/snippet}
</CardFrame>
