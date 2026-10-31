<script lang="ts">
  import { DoubleCollection } from "../winui.ts";
  import { Grid, Rectangle, TextBlock } from "winui-svelte";

  // Fades in over the page while files are dragged over the window. The
  // frame's dashes and rounded corners stay props: CSS has no WinUI meaning for
  // a Rectangle's radii or its dash array.
  interface Props {
    isShown: boolean;
  }

  const { isShown }: Props = $props();

  const dashes = [4, 3];
  const cornerRadius = 8;

  const dashArray = new DoubleCollection();
  for (const dash of dashes) {
    dashArray.append(dash);
  }
</script>

<Grid class="veil {isShown ? "veil-shown" : ""}">
  <Rectangle class="frame" radiusX={cornerRadius} radiusY={cornerRadius} strokeDashArray={dashArray} />
  <TextBlock class="card-title veil-hint">Drop a theme folder, .zip or .crx to add it</TextBlock>
</Grid>

<style>
  :global(.veil) {
    margin: 12px;
    background: var(--solid-background-fill-color-base);
    opacity: 0%;
    pointer-events: none;
    transition: opacity 120ms;
  }

  :global(.veil-shown) {
    opacity: 100%;
  }

  :global(.veil-hint) {
    place-self: center;
  }

  :global(.frame) {
    stroke: var(--accent-fill-color-default);
    stroke-width: 2px;
  }
</style>
