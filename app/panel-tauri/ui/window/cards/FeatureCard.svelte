<script lang="ts">
  import type { CardView } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import ThemingBody from "../theming/ThemingBody.svelte";
  import CardFrame from "./CardFrame.svelte";
  import { ToggleSwitch } from "winui-svelte";

  // A card with a switch at its end. The theming card carries the theme
  // library under its words.
  const { card }: { card: CardView } = $props();
  const panel = usePanel();

  // What the switch shows: the View's, until the user flips it. The binding's
  // setter hears only the user's flips, never Svelte writing the View's value.
  let isOn = $derived(card.isOn);

  function flip(next: boolean): void {
    isOn = next;
    void panel.runAndReport({
      kind: "setSwitch",
      key: card.key,
      isOn: next
    });
  }
</script>

{#snippet themeLibrary()}
  {#if card.theming !== null}
    <ThemingBody {card} theming={card.theming} />
  {/if}
{/snippet}

<CardFrame body={card.theming === null ? undefined : themeLibrary} note={card.note} wording={card.wording}>
  {#snippet end()}
    <ToggleSwitch
      automationName={card.wording.title}
      isEnabled={!card.isLocked}
      minWidth={0}
      offContent=""
      onContent=""
      bind:isOn={() => isOn, flip}
    />
  {/snippet}
</CardFrame>
