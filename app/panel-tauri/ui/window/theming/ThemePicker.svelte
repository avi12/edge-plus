<script lang="ts">
  import type { ThemeOption } from "#src/protocol.g.ts";
  import { type CardLabel, provideCardLabel } from "#src/window/cards/card-label.ts";
  import CardTitle from "#src/window/cards/CardTitle.svelte";
  import type { Snippet } from "svelte";
  import { elementRef } from "winui-svelte";
  import type { UIElement } from "winui-svelte/bindings/microsoft/ui/xaml/UIElement";
  import { ComboBox, ComboBoxItem, Grid } from "winui-svelte/components";

  interface Props {
    options: ThemeOption[];
    assignedId: string;
    pickerId: string;
    onpick: (id: string) => void;
    children: Snippet;
  }

  const { options, assignedId, pickerId, onpick, children }: Props = $props();
  const label: CardLabel = elementRef<UIElement>();
  provideCardLabel(label);

  let selectedIndex = $derived(Math.max(options.findIndex(option => option.id === assignedId), 0));

  function pick(index: number) {
    selectedIndex = index;
    const option = options.at(index);
    const hasOption = option !== undefined;
    if (hasOption) {
      onpick(option.id);
    }
  }
</script>

<Grid class="picker">
  <CardTitle>{@render children()}</CardTitle>
  <ComboBox
    class="picker-choice"
    automationId="picker-{pickerId}"
    automationLabeledBy={label.current}
    bind:selectedIndex={() => selectedIndex, pick}
  >
    {#each options as option (option.name)}
      <ComboBoxItem>{option.name}</ComboBoxItem>
    {/each}
  </ComboBox>
</Grid>

<style>
  .picker {
    grid-template-columns: 1fr auto;
    column-gap: 8px;
  }

  .picker-choice {
    grid-column: 2;
    min-width: 200px;
  }
</style>
