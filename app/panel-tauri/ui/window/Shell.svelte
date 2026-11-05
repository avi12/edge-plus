<script lang="ts">
  import type { Panel } from "./host/panel.svelte.ts";
  import PanelPage from "./page/PanelPage.svelte";
  import type { Bounds, View } from "#src/protocol.g.ts";
  import { pathToFileURL } from "node:url";
  import { Grid, Image, TextBlock, Window } from "winui-svelte/components";
  import { Backdrop } from "winui-svelte/enums";
  import type { WindowTheme } from "winui-svelte/enums";

  interface Props {
    view: View;
    panel: Panel;
    placement: Bounds | null;
    isBackground: boolean;
    theme: WindowTheme;
    refresh: () => void;
    reportBounds: (bounds: Bounds) => void;
    onclosed: () => void;
  }

  const {
    view,
    panel,
    placement,
    isBackground,
    theme,
    refresh,
    reportBounds,
    onclosed
  }: Props = $props();

  const WINDOW_WIDTH = 560;
  const WINDOW_HEIGHT = 640;
  const MINIMUM_HEIGHT = 420;
</script>

<Window
  activate={!isBackground}
  appUserModelId={view.appUserModelId}
  backdrop={Backdrop.acrylic}
  height={WINDOW_HEIGHT}
  icon={view.icon}
  isMaximizable={false}
  maxWidth={WINDOW_WIDTH}
  minHeight={MINIMUM_HEIGHT}
  minWidth={WINDOW_WIDTH}
  onactivechanged={isActive => isActive && refresh()}
  onboundschanged={reportBounds}
  {onclosed}
  placement={placement && {
    x: placement.x,
    y: placement.y,
    height: placement.height
  }}
  {theme}
  title={view.productName}
  width={WINDOW_WIDTH}
  bind:handle={panel.window}
>
  {#snippet titleBar()}
    <Grid class="title-bar">
      <Image class="title-icon" source={pathToFileURL(view.icon).href} />
      <TextBlock class="title-name">{view.productName}</TextBlock>
      <TextBlock class="title-status" class:shown={panel.isStatusShown}>{panel.statusLine}</TextBlock>
    </Grid>
  {/snippet}
  <PanelPage {panel} {view} />
</Window>

<style>
  .title-bar {
    grid-template-columns: auto auto 1fr;
    height: 40px;
    padding-left: 14px;
  }

  .title-icon {
    align-self: center;
    width: 16px;
    height: 16px;
  }

  .title-name {
    grid-column: 2;
    align-self: center;
    margin-left: 10px;
    -winui-based-on: CaptionTextBlockStyle;
  }

  .title-status {
    grid-column: 3;
    align-self: center;
    margin-left: 12px;
    color: var(--text-fill-color-secondary);
    opacity: 0%;
    transition: opacity 120ms;
    -winui-based-on: CaptionTextBlockStyle;
  }

  .title-status.shown {
    opacity: 100%;
  }
</style>
