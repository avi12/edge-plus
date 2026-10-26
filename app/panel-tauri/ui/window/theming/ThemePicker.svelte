<script lang="ts">
  import type { ThemeOption } from "../../protocol.g.ts";
  import { ComboBox, ComboBoxItem, Grid, TextBlock } from "winui-svelte";

  // The theme for one Windows mode: Edge's default theme, then the user's
  // themes, then Edge's own, beside the mode's name.
  interface Props {
    label: string;
    options: ThemeOption[];
    // The theme the core assigned to this mode.
    assignedId: string;
    onPick: (id: string) => void;
  }

  const { label, options, assignedId, onPick }: Props = $props();

  const pickerMinimumWidth = 200;

  // A theme removed meanwhile falls back to the default theme, the first
  // option. The binding's setter hears only the user's choices, never the
  // selection Svelte writes.
  let selectedIndex = $derived(Math.max(options.findIndex(option => option.id === assignedId), 0));

  function pick(index: number): void {
    selectedIndex = index;
    const option = options.at(index);
    if (option !== undefined) {
      onPick(option.id);
    }
  }
</script>

<Grid columnSpacing={8} columns="*,Auto">
  <TextBlock class="card-title" text={label} verticalAlignment="Center" />
  <ComboBox
    automationName={label}
    gridColumn={1}
    minWidth={pickerMinimumWidth}
    bind:selectedIndex={() => selectedIndex, pick}
  >
    {#each options as option (option.label)}
      <ComboBoxItem>{option.label}</ComboBoxItem>
    {/each}
  </ComboBox>
</Grid>
