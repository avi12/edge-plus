<script lang="ts">
  import { StackPanel, TextBlock, ToggleSwitch } from "svelte-winui/components";

  let isOn = $state(true);
</script>

<StackPanel padding={24} spacing={12}>
  <TextBlock fontSize={24} text="Hello from WinUI" />
  <ToggleSwitch header="Lights" bind:isOn />
  <TextBlock text={isOn ? "The lights are on" : "The lights are off"} />
</StackPanel>
