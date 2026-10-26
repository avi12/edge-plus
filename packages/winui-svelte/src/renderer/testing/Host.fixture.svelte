<script lang="ts">
  // Components for host.test.ts: a toggle that takes its state through the
  // control attachment, a raw `on-content`, a conditional toggle and a hosted
  // control.
  import { control } from "../host/control.ts";
  import type { FakeToggle } from "./fake-controls.ts";

  const { isOn, label, isShown, log, hosted } = $props();
</script>

<fake-panel class="card">
  <fake-toggle
    {@attach control<FakeToggle>(toggle => {
      log("attached");
      $effect(() => {
        toggle.isOn = isOn;
      });
      return () => log("cleanup");
    })}
    on-content={() => label}
    ontoggled={() => log("toggled")}
  />
  <fake-toggle on-content={() => "Static"} />
  {#if isShown}
    <fake-panel>
      <fake-toggle ontoggled={() => log("toggled")} />
      <host element={hosted} />
    </fake-panel>
  {/if}
</fake-panel>
