<script lang="ts">
  import { milliseconds } from "../style/layout.ts";
  import { DoubleCollection, ScalarTransition } from "../winui.ts";
  import { Grid, Rectangle, TextBlock } from "winui-svelte";

  // Fades in over the page while files are dragged over the window.
  const { isShown }: { isShown: boolean } = $props();

  const shownOpacity = 1;
  const hiddenOpacity = 0;
  const dashes = [4, 3];
  const fadeMilliseconds = 120;
  const cornerRadius = 8;

  const fade = new ScalarTransition();
  fade.duration = milliseconds(fadeMilliseconds);
  const dashArray = new DoubleCollection();
  for (const dash of dashes) {
    dashArray.append(dash);
  }
</script>

<Grid
  class="veil"
  isHitTestVisible={false}
  opacity={isShown ? shownOpacity : hiddenOpacity}
  opacityTransition={fade}
>
  <Rectangle class="frame" radiusX={cornerRadius} radiusY={cornerRadius} strokeDashArray={dashArray} />
  <TextBlock class="card-title veil-hint" text="Drop a theme folder, .zip or .crx to add it" />
</Grid>

<style>
  :global(.veil) {
    margin: 12px;
    background: var(--solid-background-fill-color-base);
  }

  :global(.veil-hint) {
    place-self: center;
  }

  :global(.frame) {
    stroke: var(--accent-fill-color-default);
    stroke-width: 2px;
  }
</style>
