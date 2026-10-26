<script lang="ts">
  import type { View } from "../../protocol.g.ts";
  import UpdateCard from "../cards/UpdateCard.svelte";
  import type { Panel } from "../host/panel.svelte.ts";
  import { providePanel } from "../host/panel.svelte.ts";
  import * as motion from "../style/motion.ts";
  import { DataPackageOperation, DragEventArgs, projectAs, StandardDataFormats } from "../winui.ts";
  import DropVeil from "./DropVeil.svelte";
  import PageFooter from "./PageFooter.svelte";
  import PanelNotices from "./PanelNotices.svelte";
  import PanelSection from "./PanelSection.svelte";
  import { Grid, ScrollViewer, StackPanel } from "svelte-winui";

  // The window's content: the update card, the notices, each section's cards
  // and the footer, with the veil a drag of files over the window shows. The
  // View is the host's state, so it changes under the page by itself.
  interface Props {
    view: View;
    panel: Panel;
  }

  const { view, panel }: Props = $props();
  // The props never change: the page is made once, with its panel.
  // svelte-ignore state_referenced_locally
  providePanel(panel);

  async function droppedPaths(event: DragEventArgs): Promise<string[]> {
    const items = await event.dataView.getStorageItemsAsync();
    const paths: string[] = [];
    for (const item of items ?? []) {
      paths.push(item.path);
    }

    return paths;
  }

  function onDragOver(_sender: unknown, event: unknown): void {
    const dragged = projectAs(event, DragEventArgs);
    const hasFiles = dragged.dataView.contains(StandardDataFormats.storageItems);
    if (!hasFiles) {
      return;
    }

    dragged.acceptedOperation = DataPackageOperation.Copy;
    panel.isVeilShown = true;
  }

  function onDragLeave(): void {
    panel.isVeilShown = false;
  }

  // The core tells a folder, .zip and .crx apart; the last failure is the one
  // shown.
  async function onDrop(_sender: unknown, event: unknown): Promise<void> {
    const dropped = projectAs(event, DragEventArgs);
    panel.isVeilShown = false;
    const hasFiles = dropped.dataView.contains(StandardDataFormats.storageItems);
    if (!hasFiles) {
      return;
    }

    panel.actionNotice = "";
    for (const path of await droppedPaths(dropped)) {
      const error = await panel.run({
        kind: "addLocalTheme",
        path
      });
      panel.report(error);
    }
  }
</script>

<Grid allowDrop={true} {onDragLeave} {onDragOver} {onDrop}>
  <ScrollViewer verticalScrollBarVisibility="Auto">
    <StackPanel padding="16,4,16,16" spacing={4}>
      {#if view.update !== null}
        <!-- A newer release brings the button back, so it is a new card. -->
        {#key JSON.stringify(view.update)}
          <UpdateCard update={view.update} />
        {/key}
      {/if}
      <PanelNotices {view} />
      <StackPanel childrenTransitions={motion.applied(motion.reposition)} spacing={20}>
        {#each view.sections as section (section.name)}
          <PanelSection {section} />
        {/each}
      </StackPanel>
      <PageFooter footer={view.footer} />
    </StackPanel>
  </ScrollViewer>
  <DropVeil hint={view.dropHint} isShown={panel.isVeilShown} />
</Grid>
