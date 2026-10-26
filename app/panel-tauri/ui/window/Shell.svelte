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
  const titleBarHeight = 40;
  const iconSize = 16;
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
    <Grid columns="Auto,Auto,*" height={titleBarHeight} padding="14,0,0,0">
      <Image height={iconSize} source={pathToFileURL(view.icon).href} verticalAlignment="Center" width={iconSize} />
      <TextBlock class="caption" gridColumn={1} margin="10,0,0,0" text={view.productName} verticalAlignment="Center" />
      <TextBlock
        class="card-description"
        gridColumn={2}
        margin="12,0,0,0"
        opacity={panel.isStatusShown ? statusShown : statusHidden}
        text={panel.statusLine}
        verticalAlignment="Center"
      />
    </Grid>
  {/snippet}
  <PanelPage {panel} {view} />
</Window>
