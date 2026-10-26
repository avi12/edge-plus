<script lang="ts">
  import { milliseconds } from "./layout.ts";
  import { DoubleCollection, ScalarTransition } from "./winui.ts";
  import { Grid, Rectangle, TextBlock } from "svelte-winui";

  // Fades in over the page while files are dragged over the window.
  const { hint, isShown }: {
    hint: string;
    isShown: boolean;
  } = $props();

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

<Grid class="veil" margin="12" opacity={isShown ? shownOpacity : hiddenOpacity} isHitTestVisible={false} opacityTransition={fade}>
  <Rectangle class="frame" strokeDashArray={dashArray} radiusX={cornerRadius} radiusY={cornerRadius} />
  <TextBlock class="card-title" horizontalAlignment="Center" verticalAlignment="Center" text={hint} />
</Grid>

<style>
  :global(.veil) {
    background: var(--solid-background-fill-color-base);
  }

  :global(.frame) {
    stroke: var(--accent-fill-color-default);
    stroke-width: 2px;
  }
</style>
