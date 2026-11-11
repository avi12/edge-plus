<script lang="ts">
  import PageFooter from "./PageFooter.svelte";
  import PanelCards from "./PanelCards.svelte";
  import PanelNotices from "./PanelNotices.svelte";
  import type { View } from "#src/protocol.g.ts";
  import UpdateCard from "#src/window/cards/UpdateCard.svelte";
  import type { Panel } from "#src/window/host/panel.svelte.ts";
  import { providePanel } from "#src/window/host/panel.svelte.ts";
  import { Grid, ScrollViewer, StackPanel } from "winui-svelte/components";
  import { ScrollBarVisibility } from "winui-svelte/enums";

  interface Props {
    view: View;
    panel: Panel;
  }

  const { view, panel }: Props = $props();
  // svelte-ignore state_referenced_locally
  providePanel(panel);
</script>

<Grid>
  <ScrollViewer verticalScrollBarVisibility={ScrollBarVisibility.Auto}>
    <StackPanel class="page">
      {#if view.update !== null}
        {#key JSON.stringify(view.update)}
          <UpdateCard update={view.update} />
        {/key}
      {/if}
      <PanelNotices {view} />
      <PanelCards {view} />
      <PageFooter footer={view.footer} />
    </StackPanel>
  </ScrollViewer>
</Grid>

<style>
  .page {
    gap: 4px;
    padding: 16px;
    padding-top: 4px;
  }
</style>
