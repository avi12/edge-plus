<script lang="ts">
  import type { UpdateView } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import CardFrame from "./CardFrame.svelte";
  import { Button } from "winui-svelte";

  // A release that is downloaded and checked, with the button that restarts
  // Edge+ into it. The page shows it once one is ready. A copy the installer
  // put under Program Files updates through the new installer, which Windows
  // asks to allow.
  const { update }: { update: UpdateView } = $props();
  const panel = usePanel();

  const how = $derived(
    update.isInstaller
      ? "Restart Edge+ to finish updating, and allow the installer when Windows asks."
      : "Restart Edge+ to finish updating."
  );

  // Pressed and not yet answered: the button says it is restarting.
  let isRestarting = $state(false);
  const label = $derived(isRestarting ? "Restarting" : "Restart to update");

  async function restart(): Promise<void> {
    // Disabled at once, so a second click cannot start a second restart.
    isRestarting = true;
    const error = await panel.restartToUpdate();
    if (error !== null) {
      // The restart failed: the button comes back, so it can be tried again.
      isRestarting = false;
      panel.report(error);
    }
  }
</script>

<CardFrame
  description="{how} Edge keeps running, and your splits and themes stay as they are."
  glyph={0xE777}
  title="Edge+ {update.version} is ready"
>
  {#snippet end()}
    <Button class="accent-button" isEnabled={!isRestarting} onClick={() => void restart()}>{label}</Button>
  {/snippet}
</CardFrame>
