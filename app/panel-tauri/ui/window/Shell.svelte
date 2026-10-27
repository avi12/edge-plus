<script lang="ts">
  import type { Bounds, View } from "../protocol.g.ts";
  import type { Panel } from "./host/panel.svelte.ts";
  import PanelPage from "./page/PanelPage.svelte";
  import * as motion from "./style/motion.ts";
  import { pathToFileURL } from "node:url";
  import { Grid, Image, TextBlock } from "winui-svelte";
  import { Window } from "winui-svelte/components";

  // The window: dressed like any-stt's panel (acrylic, the content running into
  // the title bar), so the two tools read as one family. Sizes are in
  // device-independent pixels; only the height is the user's.
  interface Props {
    view: View;
    panel: Panel;
    // A dev swap places the new window where the old one was.
    placement: Bounds | null;
    // Shown without taking focus: for unattended captures.
    isBackground: boolean;
    refresh: () => void;
    reportBounds: (bounds: Bounds) => void;
    onClosed: () => void;
  }

  const {
    view,
    panel,
    placement,
    isBackground,
    refresh,
    reportBounds,
    onClosed
  }: Props = $props();

  const windowWidth = 560;
  const windowHeight = 640;
  const minimumHeight = 420;
  const statusShown = 1;
  const statusHidden = 0;

  // Windows' animation switch and the core's View are read again whenever the
  // window is activated.
  function onActiveChanged(isActive: boolean): void {
    if (isActive) {
      motion.refresh();
      refresh();
    }
  }
</script>

<Window
  activate={!isBackground}
  appUserModelId={view.appUserModelId}
  backdrop="acrylic"
  height={windowHeight}
  icon={view.icon}
  isMaximizable={false}
  maxWidth={windowWidth}
  minHeight={minimumHeight}
  minWidth={windowWidth}
  {onActiveChanged}
  onBoundsChanged={reportBounds}
  {onClosed}
  placement={placement === null ? null : {
    x: placement.x,
    y: placement.y,
    height: placement.height
  }}
  title={view.productName}
  width={windowWidth}
  bind:handle={panel.window}
>
  {#snippet titleBar()}
    <Grid class="title-bar">
      <Image class="title-icon" source={pathToFileURL(view.icon).href} />
      <TextBlock class="caption title-name" text={view.productName} />
      <TextBlock
        class="card-description title-status"
        opacity={panel.isStatusShown ? statusShown : statusHidden}
        text={panel.statusLine}
      />
    </Grid>
  {/snippet}
  <PanelPage {panel} {view} />
</Window>

<!-- A class on a component is not an element's, so Svelte does not scope it:
     these rules are global, with names no other component uses. -->
<style>
  :global(.title-bar) {
    grid-template-columns: auto auto 1fr;
    height: 40px;
    padding-left: 14px;
  }

  :global(.title-icon) {
    align-self: center;
    width: 16px;
    height: 16px;
  }

  :global(.title-name) {
    grid-column: 2;
    align-self: center;
    margin-left: 10px;
  }

  :global(.title-status) {
    grid-column: 3;
    align-self: center;
    margin-left: 12px;
  }
</style>
