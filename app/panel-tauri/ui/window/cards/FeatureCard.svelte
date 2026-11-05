<script lang="ts">
  import CardFrame from "./CardFrame.svelte";
  import type { CardHold, CardView } from "#src/protocol.g.ts";
  import { runAndReport, usePanel } from "#src/window/host/panel.svelte.ts";
  import { edgeLead, featureTitle } from "#src/window/wording/shared-wording.ts";
  import type { Snippet } from "svelte";
  import { type GlyphInput, ToggleSwitch } from "winui-svelte/components";

  interface Props {
    card: CardView;
    glyph: GlyphInput;
    children: Snippet;
    body?: Snippet | undefined;
  }

  const { card, glyph, children, body }: Props = $props();
  const panel = usePanel();

  const note = $derived(card.hold === null ? "" : holdNote(card.hold));

  let isOn = $derived(card.isOn);

  function holdNote(hold: CardHold) {
    const title = featureTitle(card.key) ?? "";
    switch (hold.kind) {
      case "paused": {
        return `${edgeLead(hold.edge)} ${title} is paused until Edge+ supports this version.`;
      }
      case "unavailable": {
        return `${edgeLead(hold.edge)} ${title} is unavailable until Edge+ supports this version.`;
      }
      case "needs": {
        return `Turn on ${featureTitle(hold.feature)} to use this.`;
      }
    }
  }

  function flip(isNext: boolean) {
    isOn = isNext;
    void runAndReport({
      panel,
      action: {
        kind: "setSwitch",
        key: card.key,
        isOn: isNext
      }
    });
  }
</script>

<CardFrame {body} {glyph} {note}>
  {@render children()}
  {#snippet end(label)}
    <ToggleSwitch
      class="card-switch"
      automationId="switch-{card.key}"
      automationLabeledBy={label.current}
      isEnabled={!card.isLocked}
      offContent=""
      onContent=""
      bind:isOn={() => isOn, flip}
    />
  {/snippet}
</CardFrame>

<style>
  .card-switch {
    min-width: 0;
  }
</style>
