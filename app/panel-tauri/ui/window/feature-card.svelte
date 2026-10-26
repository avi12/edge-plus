<script lang="ts">
  import type { CardView } from "../protocol.g.ts";
  import CardFrame from "./card-frame.svelte";
  import { usePanel } from "./panel.svelte.ts";
  import ThemingBody from "./theming-body.svelte";
  import { ToggleSwitch } from "svelte-winui";

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

<CardFrame wording={card.wording} note={card.note} body={card.theming === null ? undefined : themeLibrary}>
  {#snippet end()}
    <ToggleSwitch
      bind:isOn={() => isOn, flip}
      onContent=""
      offContent=""
      minWidth={0}
      automationName={card.wording.title}
      isEnabled={!card.isLocked}
    />
  {/snippet}
</CardFrame>
