<script lang="ts">
  import CardFrame from "./card-frame.svelte";
  import type { FeatureCardProps } from "./feature-card.d.svelte.ts";
  import { projectAs, ToggleSwitch } from "./winui.ts";

  // A card with a switch at its end.
  const { wording, isOn, isLocked, note, body, onToggled }: FeatureCardProps = $props();

  // Svelte writing isOn raises Toggled too; only a switch that now differs
  // from isOn was flipped by the user.
  function toggled(sender: unknown): void {
    const toggle = projectAs(sender, ToggleSwitch);
    const isFlippedByUser = toggle.isOn !== isOn;
    if (isFlippedByUser) {
      onToggled(toggle.isOn);
    }
  }
</script>

{#snippet hostedBody()}
  <host element={body} />
{/snippet}

<CardFrame {wording} {note} body={body ? hostedBody : undefined}>
  {#snippet end()}
    <!-- Svelte takes every attribute that starts with "on" for an event, so
         OnContent is spelled with its capital. -->
    <toggle-switch
      On-content=""
      off-content=""
      min-width={0}
      automation-properties.name={wording.title}
      is-on={isOn}
      is-enabled={!isLocked}
      ontoggled={toggled}
    />
  {/snippet}
</CardFrame>
