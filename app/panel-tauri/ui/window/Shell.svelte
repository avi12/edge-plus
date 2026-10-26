<script lang="ts">
  import type { Bounds, View } from "../protocol.g.ts";
  import * as motion from "./motion.ts";
  import PanelPage from "./PanelPage.svelte";
  import type { Panel } from "./panel.svelte.ts";
  import { Grid, Image, TextBlock } from "svelte-winui";
  import { Window } from "svelte-winui/components";
  import { pathToFileURL } from "node:url";

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
  title={view.productName}
  backdrop="acrylic"
  icon={view.icon}
  appUserModelId={view.appUserModelId}
  width={windowWidth}
  minWidth={windowWidth}
  maxWidth={windowWidth}
  height={windowHeight}
  minHeight={minimumHeight}
  isMaximizable={false}
  placement={placement === null ? null : { x: placement.x, y: placement.y, height: placement.height }}
  activate={!isBackground}
  {onActiveChanged}
  onBoundsChanged={reportBounds}
  {onClosed}
  bind:handle={panel.window}
>
  {#snippet titleBar()}
    <Grid height={titleBarHeight} padding="14,0,0,0" columns="Auto,Auto,*">
      <Image width={iconSize} height={iconSize} verticalAlignment="Center" source={pathToFileURL(view.icon).href} />
      <TextBlock class="caption" gridColumn={1} verticalAlignment="Center" margin="10,0,0,0" text={view.productName} />
      <TextBlock
        class="card-description"
        gridColumn={2}
        verticalAlignment="Center"
        margin="12,0,0,0"
        opacity={panel.isStatusShown ? statusShown : statusHidden}
        text={panel.statusLine}
      />
    </Grid>
  {/snippet}
  <PanelPage {view} {panel} />
</Window>
