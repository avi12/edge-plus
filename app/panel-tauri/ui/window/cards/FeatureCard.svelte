<script lang="ts">
  import type { CardHold, CardView } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import { edgeLead, featureTitle } from "../wording/shared-wording.ts";
  import CardFrame from "./CardFrame.svelte";
  import type { Snippet } from "svelte";
  import { ToggleSwitch } from "winui-svelte";

  // A card with a switch at its end, saying why the switch is held when it
  // is. A feature's title is the one the core's toast uses too, so only Edge+'s
  // own cards name theirs; the theming card carries the theme library as its
  // body.
  interface Props {
    card: CardView;
    title?: string;
    description: string;
    glyph: number;
    body?: Snippet;
  }

  const { card, title, description, glyph, body }: Props = $props();
  const panel = usePanel();

  const shownTitle = $derived(title ?? featureTitle(card.key) ?? "");
  const note = $derived(card.hold === null ? "" : holdNote(card.hold));

  // What the switch shows: the View's, until the user flips it. The binding's
  // setter hears only the user's flips, never Svelte writing the View's value.
  let isOn = $derived(card.isOn);

  function holdNote(hold: CardHold): string {
    switch (hold.kind) {
      case "paused": {
        return `${edgeLead(hold.edge)} ${shownTitle} is paused until Edge+ supports this version.`;
      }
      case "unavailable": {
        return `${edgeLead(hold.edge)} ${shownTitle} is unavailable until Edge+ supports this version.`;
      }
      case "needs": {
        return `Turn on ${featureTitle(hold.feature)} to use this.`;
      }
    }
  }

  function flip(next: boolean): void {
    isOn = next;
    void panel.runAndReport({
      kind: "setSwitch",
      key: card.key,
      isOn: next
    });
  }
</script>

<CardFrame {body} {description} {glyph} {note} title={shownTitle}>
  {#snippet end()}
    <ToggleSwitch
      automationName={shownTitle}
      isEnabled={!card.isLocked}
      minWidth={0}
      offContent=""
      onContent=""
      bind:isOn={() => isOn, flip}
    />
  {/snippet}
</CardFrame>
