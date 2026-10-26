<script lang="ts">
  import { StackPanel, TextBlock, ToggleSwitch } from "svelte-winui/components";

  let isOn = $state(true);
</script>

<StackPanel padding={24} spacing={12}>
  <TextBlock text="Hello from WinUI" fontSize={24} />
  <ToggleSwitch bind:isOn header="Lights" />
  <TextBlock text={isOn ? "The lights are on" : "The lights are off"} />
</StackPanel>
