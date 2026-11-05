<script lang="ts">
  import DropVeil from "./DropVeil.svelte";
  import PageFooter from "./PageFooter.svelte";
  import PanelCards from "./PanelCards.svelte";
  import PanelNotices from "./PanelNotices.svelte";
  import type { View } from "#src/protocol.g.ts";
  import UpdateCard from "#src/window/cards/UpdateCard.svelte";
  import type { Panel } from "#src/window/host/panel.svelte.ts";
  import { providePanel } from "#src/window/host/panel.svelte.ts";
  import { Grid, ScrollViewer, StackPanel } from "winui-svelte/components";
  import { dropzone } from "winui-svelte/data-transfer";
  import type { DragEvent } from "winui-svelte/data-transfer";
  import { ScrollBarVisibility } from "winui-svelte/enums";

  interface Props {
    view: View;
    panel: Panel;
  }

  const { view, panel }: Props = $props();
  // svelte-ignore state_referenced_locally
  providePanel(panel);

  async function addDroppedThemes(event: DragEvent) {
    panel.isVeilShown = false;
    panel.actionNotice = "";
    for (const file of event.dataTransfer.files) {
      panel.report(
        await panel.run({
          kind: "addLocalTheme",
          path: file.path
        })
      );
    }
  }
</script>

<Grid
  {@attach dropzone({
    onDragStateChange: isOver => {
      panel.isVeilShown = isOver;
    },
    onDrop: addDroppedThemes
  })}
>
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
  <DropVeil isShown={panel.isVeilShown} />
</Grid>

<style>
  .page {
    gap: 4px;
    padding: 4px 16px 16px;
  }
</style>
