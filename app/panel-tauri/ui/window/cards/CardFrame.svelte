<script lang="ts">
  import { glyph as glyphText } from "../style/layout.ts";
  import type { Snippet } from "svelte";
  import {
    Border,
    FontIcon,
    Grid,
    StackPanel,
    TextBlock
  } from "winui-svelte";

  // The Windows Settings card, built by hand from WinUI's own controls rather
  // than a toolkit's SettingsCard (no UI library, to keep the supply chain
  // small): an icon, a title over a description, and one control at the end.
  // A feature card puts a switch there, the Inject card a button; a card may
  // carry a body under its words.
  interface Props {
    title: string;
    description: string;
    // A Segoe Fluent Icons code point.
    glyph: number;
    note?: string;
    end: Snippet;
    body?: Snippet;
  }

  const { title, description, glyph, note = "", end, body }: Props = $props();
</script>

<!-- The right margin on the words keeps a wrapped description off the
     control, as in Windows Settings. The end control and the body sit in a
     bare border each, which places them in the grid. -->
<Border class="card">
  <Grid class="card-grid">
    <FontIcon class="card-icon" glyph={glyphText(glyph)} />
    <StackPanel class="card-words">
      <TextBlock class="card-title" text={title} />
      <TextBlock class="card-description" text={description} />
      <TextBlock class="note" text={note} visibility={note.length > 0 ? "Visible" : "Collapsed"} />
    </StackPanel>
    <Border class="card-end">
      {@render end()}
    </Border>
    {#if body}
      <Border class="card-body">
        {@render body()}
      </Border>
    {/if}
  </Grid>
</Border>

<!-- Every colour is a WinUI theme resource, so light and dark follow Windows.
     A class on a component is not an element's, so Svelte does not scope it:
     these rules are global, with names no other component uses. -->
<style>
  :global(.card) {
    min-height: 68px;
    padding: 12px 16px;
    border: 1px solid var(--card-stroke-color-default);
    border-radius: var(--control-corner-radius);
    background: var(--card-background-fill-color-default);
  }

  :global(.card-grid) {
    grid-template-rows: auto auto;
    grid-template-columns: auto 1fr auto;
  }

  :global(.card-icon) {
    align-self: center;
    margin-right: 16px;
    font-size: 20px;
  }

  :global(.card-words) {
    grid-column: 2;
    align-self: center;
    margin-right: 16px;
  }

  :global(.card-end) {
    grid-column: 3;
    align-self: center;
  }

  :global(.card-body) {
    grid-row: 2;
    grid-column: 2 / span 2;
    margin-top: 12px;
  }

  :global(.note) {
    color: var(--system-fill-color-caution);
    text-wrap: wrap;
    -winui-based-on: CaptionTextBlockStyle;
  }
</style>
