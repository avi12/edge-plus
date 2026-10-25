<script lang="ts">
  import { milliseconds } from "./layout.ts";
  import { DoubleCollection, ScalarTransition } from "./winui.ts";

  // Fades in over the page while files are dragged over the window.
  const { hint, isShown }: {
    hint: string;
    isShown: boolean;
  } = $props();

  const shownOpacity = 1;
  const hiddenOpacity = 0;
  const dashes = [4, 3];
  const fadeMilliseconds = 120;

  const fade = new ScalarTransition();
  fade.duration = milliseconds(fadeMilliseconds);
  const dashArray = new DoubleCollection();
  for (const dash of dashes) {
    dashArray.append(dash);
  }
</script>

<grid class="veil" margin="12" opacity={isShown ? shownOpacity : hiddenOpacity} is-hit-test-visible="false" opacity-transition={fade}>
  <rectangle class="frame" stroke-dash-array={dashArray} radius-x="8" radius-y="8" />
  <text-block class="card-title" horizontal-alignment="Center" vertical-alignment="Center">{hint}</text-block>
</grid>

<style>
  .veil {
    background: var(--solid-background-fill-color-base);
  }

  .frame {
    stroke: var(--accent-fill-color-default);
    stroke-width: 2px;
  }
</style>
