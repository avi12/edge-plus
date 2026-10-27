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

<Grid class="picker">
  <TextBlock class="card-title picker-label">{label}</TextBlock>
  <ComboBox class="picker-choice" automationName={label} bind:selectedIndex={() => selectedIndex, pick}>
    {#each options as option (option.name)}
      <ComboBoxItem>{option.name}</ComboBoxItem>
    {/each}
  </ComboBox>
</Grid>

<!-- Global: a class on a component is not an element's, so Svelte does not
     scope it. -->
<style>
  :global(.picker) {
    grid-template-columns: 1fr auto;
    column-gap: 8px;
  }

  :global(.picker-label) {
    align-self: center;
  }

  :global(.picker-choice) {
    grid-column: 2;
    min-width: 200px;
  }
</style>
