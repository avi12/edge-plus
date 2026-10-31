<script lang="ts">
  import AccentButton from "./AccentButton.svelte";
  import CardDescription from "./CardDescription.svelte";
  import CardFrame from "./CardFrame.svelte";
  import CardTitle from "./CardTitle.svelte";
  import { GLYPHS } from "./glyphs.ts";
  import type { UpdateView } from "#src/protocol.g.ts";
  import { usePanel } from "#src/window/host/panel.svelte.ts";

  interface Props {
    update: UpdateView;
  }

  const { update }: Props = $props();
  const panel = usePanel();

  const instruction = $derived(
    update.isInstaller
      ? "Restart Edge+ to finish updating, and allow the installer when Windows asks."
      : "Restart Edge+ to finish updating."
  );

  let isRestarting = $state(false);
  const buttonLabel = $derived(isRestarting ? "Restarting" : "Restart to update");

  async function restart() {
    isRestarting = true;
    const error = await panel.run({ kind: "restartToUpdate" });
    const isRefused = error !== null;
    if (isRefused) {
      isRestarting = false;
      panel.report(error);
    }
  }
</script>

<CardFrame glyph={GLYPHS.update}>
  <CardTitle>Edge+ {update.version} is ready</CardTitle>
  <CardDescription>{instruction} Edge keeps running, and your splits and themes stay as they are.</CardDescription>
  {#snippet end()}
    <AccentButton automationId="restart-to-update" isEnabled={!isRestarting} onclick={restart}>{buttonLabel}</AccentButton>
  {/snippet}
</CardFrame>
