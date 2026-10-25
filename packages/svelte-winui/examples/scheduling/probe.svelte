<script lang="ts">
  import { probe } from "./probe-state.svelte.ts";
  import { TextBlock } from "../../../../app/panel-tauri/ui/window/winui.ts";
  import { withControl } from "../../../../app/panel-tauri/ui/window/svelte-renderer.ts";
  import { observe, watch } from "./watch.ts";

  // No flushSync anywhere: the question is who applies the change.
  function click(): void {
    probe.count += 1;
    watch("click");
  }
</script>

<stack-panel spacing={8} padding="12,12,12,12">
  <text-block {@attach withControl(TextBlock, control => observe(() => control.text))}>count={probe.count}</text-block>
  <button onclick={click}>Click via UIA</button>
</stack-panel>
