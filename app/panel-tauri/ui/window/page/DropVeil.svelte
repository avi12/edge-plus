<script lang="ts">
  import { Grid, Rectangle, TextBlock } from "winui-svelte/components";

  interface Props {
    isShown: boolean;
  }

  const { isShown }: Props = $props();
  const CORNER_RADIUS = 8;
  const DASH_LENGTH = 4;
  const GAP_LENGTH = 3;
  const DASH_PATTERN = [DASH_LENGTH, GAP_LENGTH];
</script>

<Grid class="veil" class:shown={isShown}>
  <Rectangle class="frame" radiusX={CORNER_RADIUS} radiusY={CORNER_RADIUS} strokeDashArray={DASH_PATTERN} />
  <TextBlock class="veil-hint">Drop a theme folder, .zip or .crx to add it</TextBlock>
</Grid>

<style>
  .veil {
    margin: 12px;
    background: var(--solid-background-fill-color-base);
    opacity: 0%;
    pointer-events: none;
    transition: opacity 120ms;
  }

  .veil.shown {
    opacity: 100%;
  }

  .veil-hint {
    place-self: center;
    -winui-based-on: BodyTextBlockStyle;
  }

  .frame {
    stroke: var(--accent-fill-color-default);
    stroke-width: 2px;
  }
</style>
