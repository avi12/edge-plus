<script lang="ts">
  import { probe } from "./probe-state.svelte.ts";
  import { observe, watch } from "./watch.ts";
  import { TextBlock } from "#winui/microsoft/ui/xaml/controls/TextBlock";
  import { withControl } from "winui-svelte";

  // No flushSync anywhere: the question is who applies the change.
  function click(): void {
    probe.count += 1;
    watch("click");
  }
</script>

<stack-panel padding="12,12,12,12" spacing={8}>
  <text-block {@attach withControl(TextBlock, control => observe(() => control.text))}>count={probe.count}</text-block>
  <button onclick={click}>Click via UIA</button>
</stack-panel>
