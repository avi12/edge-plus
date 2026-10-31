<script lang="ts">
  import { type CardLabel, provideCardLabel } from "./card-label.ts";
  import type { Snippet } from "svelte";
  import { elementRef } from "winui-svelte";
  import type { UIElement } from "winui-svelte/bindings/microsoft/ui/xaml/UIElement";
  import {
    Border,
    FontIcon,
    Grid,
    StackPanel,
    TextBlock
  } from "winui-svelte/components";
  import { Visibility } from "winui-svelte/enums";

  interface Props {
    glyph: number;
    note?: string;
    children: Snippet;
    end: Snippet<[CardLabel]>;
    body?: Snippet | undefined;
  }

  const { glyph, note = "", children, end, body }: Props = $props();
  const label = elementRef<UIElement>();
  const hasNote = $derived(note.length > 0);
  provideCardLabel(label);
</script>

<Border class="card">
  <Grid class="card-grid">
    <FontIcon class="card-icon" glyph={String.fromCodePoint(glyph)} />
    <StackPanel class="card-words">
      {@render children()}
      <TextBlock class="note" visibility={hasNote ? Visibility.Visible : Visibility.Collapsed}>{note}</TextBlock>
    </StackPanel>
    <Border class="card-end">
      {@render end(label)}
    </Border>
    {#if body}
      <Border class="card-body">
        {@render body()}
      </Border>
    {/if}
  </Grid>
</Border>

<style>
  .card {
    min-height: 68px;
    padding: 12px 16px;
    border: 1px solid var(--card-stroke-color-default);
    border-radius: var(--control-corner-radius);
    background: var(--card-background-fill-color-default);
  }

  .card-grid {
    grid-template-rows: auto auto;
    grid-template-columns: auto 1fr auto;
  }

  .card-icon {
    align-self: center;
    margin-right: 16px;
    font-size: 20px;
  }

  .card-words {
    grid-column: 2;
    align-self: center;
    margin-right: 16px;
  }

  .card-end {
    grid-column: 3;
    align-self: center;
  }

  .card-body {
    grid-row: 2;
    grid-column: 2 / span 2;
    margin-top: 12px;
  }

  .note {
    color: var(--system-fill-color-caution);
    text-wrap: wrap;
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
