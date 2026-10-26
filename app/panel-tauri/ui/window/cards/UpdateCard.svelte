<script lang="ts">
  import type { UpdateView } from "../../protocol.g.ts";
  import { usePanel } from "../host/panel.svelte.ts";
  import CardFrame from "./CardFrame.svelte";
  import { Button } from "winui-svelte";

  // A release that is downloaded and checked, with the button that restarts
  // Edge+ into it. The page shows it once one is ready.
  const { update }: { update: UpdateView } = $props();
  const panel = usePanel();

  // Pressed and not yet answered: the button says it is restarting.
  let isRestarting = $state(false);
  const label = $derived(isRestarting ? update.restarting : update.button);

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

<CardFrame wording={update.wording}>
  {#snippet end()}
    <Button class="accent-button" isEnabled={!isRestarting} onClick={() => void restart()}>{label}</Button>
  {/snippet}
</CardFrame>
